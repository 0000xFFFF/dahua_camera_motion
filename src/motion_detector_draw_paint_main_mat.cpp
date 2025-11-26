#include "motion_detector.hpp"

cv::Mat MotionDetector::draw_paint_main_mat_all()
{
    bool layout_changed = m_layout_changed;
    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = i + 1;
            cv::UMat mat = get_frame(ch, layout_changed).getUMat(cv::ACCESS_READ);
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::resize(mat, m_canv2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_paint_info_motion_region(m_canv2.getMat(cv::ACCESS_RW), x, y, w, h);
            }
        }
    });

    m_layout_changed = false;
    return m_canv2.getMat(cv::ACCESS_READ);
}

cv::Mat MotionDetector::draw_paint_main_mat_sort()
{
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 3;
    size_t h = m_display_height / 3;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            int ch = vec[i];
            cv::UMat mat = get_frame(ch, layout_changed).getUMat(cv::ACCESS_READ);
            if (mat.empty()) { continue; }

            int row = i / 3;
            int col = i % 3;
            int x = col * w;
            int y = row * h;
            cv::Rect roi(x, y, w, h);
            cv::resize(mat, m_canv2(cv::Rect(x, y, w, h)), cv::Size(w, h));
            if (ch == m_current_channel) {
                draw_paint_info_motion_region(m_canv2.getMat(cv::ACCESS_RW), x, y, w, h);
            }
        }
    });

    m_layout_changed = false;
    return m_canv2.getMat(cv::ACCESS_READ);
}

cv::Mat MotionDetector::draw_paint_main_mat_king()
{
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 4;
    size_t h = m_display_height / 4;

    std::vector vec = m_king_chain.get();

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::UMat mat = get_frame(vec[i], layout_changed).getUMat(cv::ACCESS_READ);
            if (mat.empty()) { continue; }

            switch (i) {
                case 0:
                    {
                        size_t w0 = w * 3;
                        size_t h0 = h * 3;
                        cv::resize(mat, m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                        draw_paint_info_motion_region(m_canv1.getMat(cv::ACCESS_RW), 0, 0, w0, h0);
                        break;
                    }
                case 1: cv::resize(mat, m_canv1(cv::Rect(3 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                case 2: cv::resize(mat, m_canv1(cv::Rect(3 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                case 3: cv::resize(mat, m_canv1(cv::Rect(3 * w, 2 * h, w, h)), cv::Size(w, h)); break;

                case 4: cv::resize(mat, m_canv1(cv::Rect(0 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 5: cv::resize(mat, m_canv1(cv::Rect(1 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 6: cv::resize(mat, m_canv1(cv::Rect(2 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                case 7: cv::resize(mat, m_canv1(cv::Rect(3 * w, 3 * h, w, h)), cv::Size(w, h)); break;
            }
        }
    });

    m_layout_changed = false;
    return m_canv1.getMat(cv::ACCESS_READ);
}

cv::Mat MotionDetector::draw_paint_main_mat_top()
{
    bool layout_changed = m_layout_changed;

    size_t w = m_display_width / 4;
    size_t h = m_display_height / 4;

    std::vector<int> active_channels;
    for (int i = 1; i <= CHANNEL_COUNT; i++) {
        if (i != m_current_channel) {
            active_channels.push_back(i);
        }
    }

    cv::parallel_for_(cv::Range(0, CHANNEL_COUNT), [&](const cv::Range& range) {
        for (int i = range.start; i < range.end; i++) {
            cv::UMat mat;

            if (i == 0) {
                mat = get_frame(m_current_channel, layout_changed).getUMat(cv::ACCESS_READ);
                if (mat.empty()) { continue; }

                size_t w0 = w * 3;
                size_t h0 = h * 3;
                cv::resize(mat, m_canv1(cv::Rect(0 * w, 0 * h, w0, h0)), cv::Size(w0, h0));
                draw_paint_info_motion_region(m_canv1.getMat(cv::ACCESS_RW), 0, 0, w0, h0);
            }
            else {
                // Other slots are from active_channels (excluding m_current_channel)
                int channel = active_channels[i - 1]; // Skip 0th slot
                mat = get_frame(channel, layout_changed).getUMat(cv::ACCESS_READ);
                if (mat.empty()) { continue; }

                // Layout for circular arrangement
                switch (i) {
                    case 1: cv::resize(mat, m_canv1(cv::Rect(3 * w, 0 * h, w, h)), cv::Size(w, h)); break;
                    case 2: cv::resize(mat, m_canv1(cv::Rect(3 * w, 1 * h, w, h)), cv::Size(w, h)); break;
                    case 3: cv::resize(mat, m_canv1(cv::Rect(3 * w, 2 * h, w, h)), cv::Size(w, h)); break;
                    case 7: cv::resize(mat, m_canv1(cv::Rect(0 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 6: cv::resize(mat, m_canv1(cv::Rect(1 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 5: cv::resize(mat, m_canv1(cv::Rect(2 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                    case 4: cv::resize(mat, m_canv1(cv::Rect(3 * w, 3 * h, w, h)), cv::Size(w, h)); break;
                }
            }
        }
    });

    m_layout_changed = false;
    return m_canv1.getMat(cv::ACCESS_READ);
}
