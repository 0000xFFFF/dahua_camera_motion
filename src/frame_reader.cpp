#include "debug.hpp"
#include "globals.hpp"
#include "utils.hpp"
#include <atomic>
#include <chrono>
#include <opencv2/opencv.hpp>
#include <string>
#include <sys/types.h>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include "frame_reader.hpp"

FrameReader::FrameReader(int channel,
                         const std::string& ip,
                         const std::string& username,
                         const std::string& password,
                         int subtype,
                         bool autostart,
                         bool has_placeholder)
    :

      m_ip(ip),
      m_username(username),
      m_password(password),
      m_channel(channel),
      m_subtype(subtype)
{

    if (has_placeholder) {
        put_placeholder();
    }

    if (autostart) {
        start();
    }
}

void FrameReader::put_placeholder()
{
    // Create a black placeholder image
    cv::Mat placeholder_cpu(cv::Size(1000, 1000), CV_8UC3, cv::Scalar(0, 0, 0));

    const cv::Scalar text_color(255, 255, 255);
    const double font_scale = 10;
    const int font_thickness = 20;

    std::string text = std::to_string(m_channel);

    int baseline = 0;
    cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseline);
    cv::Point text_origin((placeholder_cpu.cols - text_size.width) / 2, (placeholder_cpu.rows + text_size.height) / 2);

    cv::putText(placeholder_cpu, text, text_origin, cv::FONT_HERSHEY_SIMPLEX, font_scale, text_color, font_thickness);

    // Draw white border
    cv::rectangle(placeholder_cpu, cv::Rect(0, 0, placeholder_cpu.cols, placeholder_cpu.rows), cv::Scalar(255, 255, 255), 3);

    cv::UMat placeholder;
    placeholder_cpu.copyTo(placeholder);
    m_frame_buffer.push(placeholder);
    m_frame_dbuffer.update(placeholder);
}

cv::UMat FrameReader::get_latest_frame(bool no_empty_frame)
{
    if (no_empty_frame) { return m_frame_dbuffer.get(); }
    auto frame = m_frame_buffer.pop();
    return frame ? *frame : cv::UMat();
}

double FrameReader::get_fps()
{
    return captured_fps.load();
}

void FrameReader::stop()
{
    m_cleaning = true;
    D(std::cout << "[" << m_channel << "] reader stop" << std::endl);
    m_running = false;
    m_cv.notify_one();

    if (m_thread.joinable()) {
        D(std::cout << "[" << m_channel << "] reader join" << std::endl);
        m_thread.join();
        D(std::cout << "[" << m_channel << "] reader joined" << std::endl);
    }
    D(std::cout << "[" << m_channel << "] reader stopped" << std::endl);
    m_cleaning = false;
}

std::string FrameReader::construct_rtsp_url(const std::string& ip, const std::string& username,
                                            const std::string& password, int subtype)
{
    int st = (subtype && m_channel != 0) ? 1 : 0;
    return "rtsp://" + username + ":" + password + "@" + ip +
           ":554/cam/realmonitor?channel=" + std::to_string(m_channel) +
           "&subtype=" + std::to_string(st);
}

// Place this helper somewhere in the file scope (above the method)
static enum AVPixelFormat get_vaapi_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts)
{
    UNUSED(ctx);
    const enum AVPixelFormat* p = pix_fmts;
    while (*p != AV_PIX_FMT_NONE) {
        if (*p == AV_PIX_FMT_VAAPI) {
            return AV_PIX_FMT_VAAPI;
        }
        ++p;
    }
    // fallback to first available
    return pix_fmts[0];
}

void FrameReader::connect_and_read()
{
    bool connected = false;

    std::cout << "start capture: " << m_channel << std::endl;
    std::string rtsp_url = construct_rtsp_url(m_ip, m_username, m_password, m_subtype);

    avformat_network_init();
    AVFormatContext* formatCtx = avformat_alloc_context();

    set_thread_affinity(m_channel % std::thread::hardware_concurrency());

    // Retry loop for connection
    while (!connected) {
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "3000000", 0); // 3s timeout (microseconds)
        av_dict_set(&options, "fflags", "discardcorrupt", 0);
        av_dict_set(&options, "packet_buffer_size", "2048000", 0);

        if (avformat_open_input(&formatCtx, rtsp_url.c_str(), NULL, &options) == 0) {
            if (avformat_find_stream_info(formatCtx, NULL) >= 0) {
                for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
                    if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                        connected = true;
                        break;
                    }
                }
            }
        }

        if (!connected) {
            if (formatCtx) avformat_close_input(&formatCtx);
            std::cerr << "Failed to connect or find video stream for channel " << m_channel << std::endl;

            {
                std::unique_lock<std::mutex> lock(m_mtx);
                m_cv.wait_for(lock, std::chrono::milliseconds(CONN_RETRY_MS), [&] { return !m_running; });
            }
        }

        if (!m_running) {
            D(std::cout << "Exiting readFrames() thread for channel " << m_channel << std::endl);
            avformat_close_input(&formatCtx);
            return;
        }
    }

    // find video stream index
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }
    if (videoStreamIndex == -1) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("No video stream found for channel " + std::to_string(m_channel));
    }

    AVCodecParameters* codecParams = formatCtx->streams[videoStreamIndex]->codecpar;

    // Use the standard decoder (not vaapi-specific decoder names)
    const AVCodec* decoder = avcodec_find_decoder(codecParams->codec_id);
    if (!decoder) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("No suitable decoder found for channel " + std::to_string(m_channel));
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("Failed to alloc codec context for channel " + std::to_string(m_channel));
    }
    avcodec_parameters_to_context(codecCtx, codecParams);

    // low-latency / threading hints
    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    // Use more threads for software decoding
    codecCtx->thread_count = 0; // 0 = auto-detect optimal thread count
    codecCtx->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    codecCtx->skip_frame = AVDISCARD_DEFAULT;

    // Attempt to create a VAAPI device for HW acceleration
    AVBufferRef* hw_device_ctx = nullptr;
    if (codecParams->codec_id == AV_CODEC_ID_H264 || codecParams->codec_id == AV_CODEC_ID_HEVC) {
        // Try default first (NULL), then fall back to common paths
        int err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                                         NULL, NULL, 0); // NULL = auto-detect
        if (err < 0) {
            // Try explicit paths if auto-detect fails
            err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI,
                                         "/dev/dri/renderD128", NULL, 0);
        }

        if (err == 0) {
            codecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            codecCtx->get_format = get_vaapi_format;
            std::cout << "VAAPI hardware acceleration enabled for channel " << m_channel << std::endl;
        }
        else {
            char errbuf[256];
            av_strerror(err, errbuf, sizeof(errbuf));
            std::cerr << "Warning: VAAPI init failed for channel " << m_channel
                      << " (" << errbuf << ") -- using software decode." << std::endl;
        }
    }

    // codec options (don't set encoder-only options)
    AVDictionary* codecOptions = nullptr;

    // open codec
    if (avcodec_open2(codecCtx, decoder, &codecOptions) < 0) {
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        throw std::runtime_error("Could not open codec for channel " + std::to_string(m_channel));
    }

    // prepare frame/packet
    AVPacket packet;
    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        throw std::runtime_error("Failed to allocate AVFrame");
    }

    // Cache sws context to avoid recreation per frame
    SwsContext* swsCtx = nullptr;
    int cached_w = 0, cached_h = 0;
    AVPixelFormat cached_fmt = AV_PIX_FMT_NONE;

    std::cout << "connected: " << m_channel << " -- " << codecCtx->width << "x" << codecCtx->height << std::endl;

    // prepare an output cv::Mat placeholder (will be resized per-frame if needed)
    cv::Mat image_cpu;

    int i = 0;
    int framesDecoded = 0;
    auto start_time = std::chrono::high_resolution_clock::now();
    int64_t last_pts = AV_NOPTS_VALUE;


    // main loop
    while (m_running) {
        if (av_read_frame(formatCtx, &packet) < 0) {
            // EOF or error: small sleep to avoid tight loop
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        if (packet.stream_index == videoStreamIndex) {
            if (avcodec_send_packet(codecCtx, &packet) < 0) {
                av_packet_unref(&packet);
                continue;
            }
        }
        av_packet_unref(&packet);

        // Receive all available frames
        while (avcodec_receive_frame(codecCtx, frame) == 0) {
            // skip initial frames if needed to allow decoder warm-up
            if (++framesDecoded < 5) {
                av_frame_unref(frame);
                continue;
            }

            // skip non-increasing PTS frames
            if (last_pts != AV_NOPTS_VALUE && frame->pts <= last_pts) {
                av_frame_unref(frame);
                continue;
            }
            last_pts = frame->pts;

            // If this is a hardware frame (VAAPI), transfer it to a CPU-accessible frame
            AVFrame* cpu_frame = nullptr;
            if (frame->format == AV_PIX_FMT_VAAPI) {
                cpu_frame = av_frame_alloc();
                if (av_hwframe_transfer_data(cpu_frame, frame, 0) < 0) {
                    av_frame_free(&cpu_frame);
                    cpu_frame = nullptr; // fall back to using `frame` (software path) if transfer fails
                }
            }

            AVFrame* used_frame = cpu_frame ? cpu_frame : frame;

            // width/height for convenience
            int w = used_frame->width;
            int h = used_frame->height;

            // Update cached sws context if format/dimensions changed
            if (!swsCtx || w != cached_w || h != cached_h || (AVPixelFormat)used_frame->format != cached_fmt) {
                if (swsCtx) sws_freeContext(swsCtx);
                swsCtx = sws_getContext(
                    w, h, (AVPixelFormat)used_frame->format,
                    w, h, AV_PIX_FMT_BGR24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                cached_w = w;
                cached_h = h;
                cached_fmt = (AVPixelFormat)used_frame->format;
            }

            if (swsCtx) {
                // Create Mat, do sws_scale, then upload to UMat once
                if (image_cpu.rows != h || image_cpu.cols != w) {
                    image_cpu.create(h, w, CV_8UC3);
                }
                uint8_t* dst[1] = {image_cpu.data};
                int dst_linesize[1] = {static_cast<int>(image_cpu.step[0])};
                sws_scale(swsCtx, used_frame->data, used_frame->linesize, 0, h, dst, dst_linesize);

                // Upload to GPU once
                cv::UMat image_gpu;
                image_cpu.copyTo(image_gpu);

                m_frame_buffer.push(image_gpu);
                m_frame_dbuffer.update(image_gpu);
                m_active = true;
            }
            else {
                std::cerr << "Failed to create sws context for channel " << m_channel << "." << std::endl;
            }

            // cleanup cpu_frame if allocated during hw transfer
            if (cpu_frame) {
                av_frame_free(&cpu_frame);
            }

            av_frame_unref(frame);

            // FPS tracking (same as original logic)
            i++;
            if (i % 30 == 0) {
                auto end_time = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end_time - start_time;
                double fps = 30.0 / elapsed.count();
                captured_fps = fps;
#ifdef DEBUG_FPS
                if (i % 100 == 0) {
                    std::cout << "Channel " << m_channel << " Frame Rate: " << fps << " FPS" << std::endl;
                }
#endif
                start_time = std::chrono::high_resolution_clock::now();
            }
        } // avcodec_receive_frame loop
    } // main m_running loop

    m_active = false;

    // cleanup
    if (swsCtx) sws_freeContext(swsCtx);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
    avformat_close_input(&formatCtx);

    D(std::cout << "Exiting readFrames() thread for channel " << m_channel << std::endl);
}

void FrameReader::start()
{
    if (!m_running && !m_cleaning) {
        m_running = true;
        m_thread = std::thread([this] { connect_and_read(); });
    }
}

bool FrameReader::is_running()
{
    return m_running.load();
}

bool FrameReader::is_active()
{
    return m_active.load();
}
