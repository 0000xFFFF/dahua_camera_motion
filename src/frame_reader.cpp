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

    cv::Mat placeholder(cv::Size(1000, 1000), CV_8UC3);
    cv::rectangle(placeholder, cv::Rect(0, 0, placeholder.size().width, placeholder.size().height), cv::Scalar(255, 255, 255), 3);

    const cv::Scalar text_color(255, 255, 255);
    const double font_scale = 10;
    const int font_thickness = 20;

    std::string text = std::to_string(m_channel);

    // Get the text size to calculate the correct position
    int baseline = 0;
    cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseline);

    // Calculate the position for the text to be centered
    cv::Point text_origin((placeholder.cols - text_size.width) / 2, (placeholder.rows + text_size.height) / 2);

    // Draw the text on the placeholder
    cv::putText(placeholder,
                text,
                text_origin,
                cv::FONT_HERSHEY_SIMPLEX,
                font_scale,
                text_color,
                font_thickness);

    m_frame_buffer.push(placeholder);
    m_frame_dbuffer.update(placeholder);
}

cv::Mat FrameReader::get_latest_frame(bool no_empty_frame)
{
    if (no_empty_frame) { return m_frame_dbuffer.get(); }
    auto frame = m_frame_buffer.pop();
    return frame ? *frame : cv::Mat();
}

void FrameReader::enable_sleep()
{
    m_sleep = true;
}

void FrameReader::disable_sleep()
{
    m_sleep = false;
    m_cv.notify_one();
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

// Replacement for FrameReader::connect_and_read()
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
        av_dict_set(&options, "fflags", "nobuffer", 0);
        av_dict_set(&options, "buffer_size", "1024000", 0);
        av_dict_set(&options, "max_delay", "500000", 0);

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

    // choose VAAPI decoder name for common codecs; otherwise fallback
    const AVCodec* decoder = nullptr;
    if (codecParams->codec_id == AV_CODEC_ID_H264) {
        decoder = avcodec_find_decoder_by_name("h264_vaapi");
    }
    else if (codecParams->codec_id == AV_CODEC_ID_HEVC) {
        decoder = avcodec_find_decoder_by_name("hevc_vaapi");
    }
    if (!decoder) {
        // fallback to generic decoder (may not be hw-accelerated)
        decoder = avcodec_find_decoder(codecParams->codec_id);
        if (!decoder) {
            avformat_close_input(&formatCtx);
            throw std::runtime_error("No suitable decoder found for channel " + std::to_string(m_channel));
        }
    }

    AVCodecContext* codecCtx = avcodec_alloc_context3(decoder);
    if (!codecCtx) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("Failed to alloc codec context for channel " + std::to_string(m_channel));
    }
    avcodec_parameters_to_context(codecCtx, codecParams);

    // low-latency / threading hints
    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx->thread_count = 1; // safe low-latency
    codecCtx->skip_frame = AVDISCARD_DEFAULT;

    // Attempt to create a VAAPI device and negotiate VAAPI pixel format
    AVBufferRef* hw_device_ctx = nullptr;
    if (decoder->id == AV_CODEC_ID_H264 || decoder->id == AV_CODEC_ID_HEVC ||
        strcmp(decoder->name, "h264_vaapi") == 0 || strcmp(decoder->name, "hevc_vaapi") == 0) {
        if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0) == 0) {
            codecCtx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
            codecCtx->get_format = get_vaapi_format;
        }
        else {
            // If VAAPI device creation fails, continue with software path (still better than nothing)
            std::cerr << "Warning: av_hwdevice_ctx_create(VAAPI) failed for channel " << m_channel << " -- falling back to software decode." << std::endl;
        }
    }

    // codec options
    AVDictionary* codecOptions = nullptr;
    av_dict_set(&codecOptions, "tune", "zerolatency", 0);
    av_dict_set(&codecOptions, "preset", "ultrafast", 0);
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

    std::cout << "connected: " << m_channel << " -- " << codecCtx->width << "x" << codecCtx->height << std::endl;

    // prepare an output cv::Mat placeholder (will be updated each frame)
    cv::Mat image_cpu; // will be resized per-frame if needed

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

            // If this is a hardware frame (VAAPI), transfer it to a CPU-accessible frame (likely NV12)
            AVFrame* cpu_frame = nullptr;
            bool did_hw_transfer = false;
            if (frame->format == AV_PIX_FMT_VAAPI) {
                cpu_frame = av_frame_alloc();
                if (av_hwframe_transfer_data(cpu_frame, frame, 0) < 0) {
                    // transfer failed; free and fallback
                    av_frame_free(&cpu_frame);
                    cpu_frame = nullptr;
                }
                else {
                    did_hw_transfer = true;
                }
            }

            // If not a hw frame OR hw transfer failed, try to use the received frame directly (software decode)
            AVFrame* used_frame = cpu_frame ? cpu_frame : frame;

            // At this point 'used_frame' should be a software frame (e.g., NV12 or YUV420P)
            // Preferred path: if format is NV12 (common from VAAPI), use OpenCV UMat + cvtColor (OpenCL path)
            if (used_frame->format == AV_PIX_FMT_NV12 || used_frame->format == AV_PIX_FMT_YUV420P) {
                int w = used_frame->width;
                int h = used_frame->height;

                // Create a single-channel Mat with rows = h * 3 / 2 for NV12 layout (Y plane + interleaved UV)
                if (used_frame->format == AV_PIX_FMT_NV12) {
                    // NV12 layout: Y plane (h x w) followed by interleaved UV (h/2 x w)
                    cv::Mat yuv(h + h / 2, w, CV_8UC1, used_frame->data[0], used_frame->linesize[0]);
                    // copy to UMat (this uploads to GPU via OpenCL if available)
                    cv::UMat yuvU;
                    yuv.copyTo(yuvU);

                    // convert NV12 to BGR on GPU (if OpenCL enabled)
                    cv::UMat bgrU;
                    cv::cvtColor(yuvU, bgrU, cv::COLOR_YUV2BGR_NV12);

                    // convert back to CPU Mat (only if your buffer expects CPU Mat)
                    bgrU.copyTo(image_cpu);

                    // push result
                    m_frame_buffer.push(image_cpu.clone());
                    m_frame_dbuffer.update(image_cpu);
                }
                else { // AV_PIX_FMT_YUV420P
                    // YUV420P: three separate planes; construct Mat and convert
                    // Create a contiguous buffer in YUV420P packed form (rows = h * 3 / 2)
                    // Simpler safe path: use sws_scale to convert YUV420P -> BGR24 into image_cpu
                    SwsContext* tmp_sws = sws_getContext(
                        used_frame->width, used_frame->height, (AVPixelFormat)used_frame->format,
                        used_frame->width, used_frame->height, AV_PIX_FMT_BGR24,
                        SWS_BILINEAR, NULL, NULL, NULL);

                    if (!tmp_sws) {
                        std::cerr << "Failed to create sws context for YUV420P fallback." << std::endl;
                    }
                    else {
                        image_cpu.create(used_frame->height, used_frame->width, CV_8UC3);
                        uint8_t* dst[1] = {image_cpu.data};
                        int dst_linesize[1] = {static_cast<int>(image_cpu.step[0])};
                        sws_scale(tmp_sws, used_frame->data, used_frame->linesize, 0, used_frame->height, dst, dst_linesize);
                        sws_freeContext(tmp_sws);

                        m_frame_buffer.push(image_cpu.clone());
                        m_frame_dbuffer.update(image_cpu);
                    }
                }
            }
            else {
                // If we get some other pixel format, convert with sws_scale to BGR24 (safe path)
                SwsContext* swsCtx = sws_getContext(
                    used_frame->width, used_frame->height, (AVPixelFormat)used_frame->format,
                    used_frame->width, used_frame->height, AV_PIX_FMT_BGR24,
                    SWS_BILINEAR, NULL, NULL, NULL);

                if (swsCtx) {
                    image_cpu.create(used_frame->height, used_frame->width, CV_8UC3);
                    uint8_t* data[1] = {image_cpu.data};
                    int linesize[1] = {static_cast<int>(image_cpu.step[0])};
                    sws_scale(swsCtx, used_frame->data, used_frame->linesize, 0, used_frame->height, data, linesize);
                    sws_freeContext(swsCtx);

                    m_frame_buffer.push(image_cpu.clone());
                    m_frame_dbuffer.update(image_cpu);
                }
                else {
                    std::cerr << "Unsupported pixel format and sws_getContext failed." << std::endl;
                }
            }

            // cleanup cpu_frame if we allocated one during hw transfer
            if (cpu_frame) {
                av_frame_free(&cpu_frame);
            }

            av_frame_unref(frame);

            // FPS tracking (same as your original logic)
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
