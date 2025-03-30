#include "debug.hpp"
#include "globals.hpp"
#include "utils.h"
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

FrameReader::FrameReader(int ch, const std::string& ip,
                         const std::string& username, const std::string& password)
    :

      m_ip(ip),
      m_username(username),
      m_password(password),
      m_channel(ch)
{

    put_placeholder();

    m_thread = std::thread([this]() { connect_and_read(); });
}

void FrameReader::put_placeholder()
{

    cv::Mat placeholder(cv::Size(1000, 1000), CV_8UC3);
    cv::copyMakeBorder(placeholder, placeholder, 2, 2, 2, 2, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255));

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
}

cv::Mat FrameReader::get_latest_frame()
{
    auto frame = m_frame_buffer.pop();
    return frame ? *frame : cv::Mat();
}

double FrameReader::get_fps()
{
    return captured_fps.load();
}

void FrameReader::stop()
{
    D(std::cout << "[" << m_channel << "] reader stop" << std::endl);
    m_running = false;
    if (m_thread.joinable()) {
        D(std::cout << "[" << m_channel << "] reader join" << std::endl);
        m_thread.join();
        D(std::cout << "[" << m_channel << "] reader joined" << std::endl);
    }
    D(std::cout << "[" << m_channel << "] reader stopped" << std::endl);
}

std::string FrameReader::construct_rtsp_url(const std::string& ip, const std::string& username,
                                            const std::string& password)
{
    int subtype = (USE_SUBTYPE1 && m_channel != 0) ? 1 : 0;
    return "rtsp://" + username + ":" + password + "@" + ip +
           ":554/cam/realmonitor?channel=" + std::to_string(m_channel) +
           "&subtype=" + std::to_string(subtype);
}

void FrameReader::connect_and_read()
{
    set_thread_affinity(m_channel % std::thread::hardware_concurrency()); // Assign different core to each camera

    D(std::cout << "start capture: " << m_channel << std::endl);
    std::string rtsp_url = construct_rtsp_url(m_ip, m_username, m_password);

    avformat_network_init();
    AVFormatContext* formatCtx = avformat_alloc_context();

    if (avformat_open_input(&formatCtx, rtsp_url.c_str(), NULL, NULL) != 0) {
        throw std::runtime_error("Failed to open RTSP stream for channel " + std::to_string(m_channel));
    }

    if (avformat_find_stream_info(formatCtx, NULL) < 0) {
        avformat_close_input(&formatCtx);
        throw std::runtime_error("Failed to retrieve stream info for channel " + std::to_string(m_channel));
    }

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
    const AVCodec* codec = avcodec_find_decoder(codecParams->codec_id);
    AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codecCtx, codecParams);
    avcodec_open2(codecCtx, codec, NULL);

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    SwsContext* swsCtx = sws_getContext(
        codecCtx->width, codecCtx->height, codecCtx->pix_fmt,
        codecCtx->width, codecCtx->height, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, NULL, NULL, NULL);

    cv::Mat image(codecCtx->height, codecCtx->width, CV_8UC3);

    D(std::cout << "connected: " << m_channel << std::endl);

    int i = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    while (m_running) {
        if (av_read_frame(formatCtx, packet) < 0) {
            continue;
        }

        if (packet->stream_index == videoStreamIndex) {
            avcodec_send_packet(codecCtx, packet);
            if (avcodec_receive_frame(codecCtx, frame) == 0) {
                uint8_t* data[1] = {image.data};
                int linesize[1] = {3 * codecCtx->width};
                sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height, data, linesize);

                m_frame_buffer.push(std::move(image.clone()));

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
            }
        }

        av_packet_unref(packet);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&codecCtx);
    avformat_close_input(&formatCtx);
    sws_freeContext(swsCtx);

    D(std::cout << "Exiting readFrames() thread for channel " << m_channel << std::endl);
}
