#include "rtspReader.h"     // Header khai báo lớp RTSPReader
#include <iostream>         // Sử dụng cout, cerr
#include <chrono>           // Dùng đo thời gian FPS

RTSPReader::RTSPReader(const std::string& rtsp_URL)                        // Constructor khởi tạo đối tượng với URL
    : url(rtsp_URL), running(false), pipeline(nullptr), appsink(nullptr) { // Khởi tạo biến thành viên
    gst_init(nullptr, nullptr);                                            // Khởi tạo GStreamer
}

RTSPReader::~RTSPReader() {
    stop();                 // Gọi stop() khi huỷ đối tượng
}

void RTSPReader::stop() {
    running = false;                              // Dừng vòng lặp luồng
    if (readThread.joinable()) readThread.join(); // Đợi luồng kết thúc

    if (pipeline) {                                        // Nếu pipeline còn tồn tại
        gst_element_set_state(pipeline, GST_STATE_NULL);   // Tắt pipeline
        gst_object_unref(pipeline);                        // Giải phóng pipeline
        pipeline = nullptr;                                // Reset con trỏ
    }

    if (appsink) {                                         // Nếu appsink còn
        gst_object_unref(appsink);                         // Giải phóng appsink
        appsink = nullptr;                                 // Reset
    }

    cv::destroyAllWindows();                               // Đóng tất cả cửa sổ OpenCV
}

bool RTSPReader::open() {
    std::string pipelineStr =                                           // Chuỗi mô tả pipeline GStreamer
        "rtspsrc location=" + url + " latency=200 protocols=tcp ! "     // Nguồn RTSP TCP
        "queue max-size-buffers=100 leaky=downstream ! "                // Bộ đệm rò xuống
        "rtph264depay ! "                                               // Giải nén RTP H264
        "queue ! " 
        "h264parse config-interval=1 ! "                                // Parser H264
        "queue max-size-buffers=100 leaky=downstream ! "
        "mppvideodec ! "                                                // Giải mã bằng MPP (zero-copy)
        "queue max-size-buffers=100 leaky=downstream ! "
        "videoscale method=1 ! "                                        // Resize hình
        "videorate ! "
        "video/x-raw, format=NV12, width=840, height=720, framerate = 25/1 , memory=DMABuf ! " // Đầu ra raw NV12 với DMA
        "appsink name=mysink max-buffers=1 drop=true sync=false";       // Gắn vào appsink để xử lý ảnh

    GError* error = nullptr;                                  // Để bắt lỗi parse
    pipeline = gst_parse_launch(pipelineStr.c_str(), &error); // Parse pipeline
    if (!pipeline) {                                          // Nếu lỗi
        std::cerr << "[ERROR] Tạo pipeline thất bại: " << error->message << std::endl; // In lỗi
        g_error_free(error);                                                           // Giải phóng lỗi
        return false;                                                                  // Thất bại
    }

    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink"); // Lấy appsink
    if (!appsink) { // Không tìm thấy appsink
        std::cerr << "[ERROR] Không tìm thấy appsink.\n"; // Báo lỗi
        return false; // Thất bại
    }

    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink), false); // Tắt tín hiệu
    gst_app_sink_set_drop(GST_APP_SINK(appsink), true); // Bỏ frame cũ
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1); // Chỉ giữ 1 buffer

    gst_element_set_state(pipeline, GST_STATE_PLAYING); // Chạy pipeline
    std::cout << "✅ RTSP kết nối thành công. DMA-BUF đang hoạt động.\n"; // Thông báo
    return true; // Thành công
}

int RTSPReader::getCurrentDmaFd() {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink)); // Kéo sample mới
    if (!sample) return -1; // Nếu không có trả về -1

    GstBuffer* buffer = gst_sample_get_buffer(sample); // Lấy buffer từ sample
    if (!buffer) {
        gst_sample_unref(sample); // Giải phóng sample
        return -1; // Lỗi
    }

    GstMemory* mem = gst_buffer_peek_memory(buffer, 0); // Lấy memory
    if (!gst_is_dmabuf_memory(mem)) { // Không phải memory DMA
        std::cerr << "[ERROR] Memory không phải DMABuf.\n"; // Báo lỗi
        gst_sample_unref(sample); // Giải phóng sample
        return -1; // Lỗi
    }

    int fd = gst_dmabuf_memory_get_fd(mem); // Lấy file descriptor
    gst_sample_unref(sample); // Giải phóng sample
    return fd; // Trả về fd
}

cv::Mat RTSPReader::gstSampleToMat(GstSample* sample) {
    GstBuffer* buffer = gst_sample_get_buffer(sample); // Lấy buffer
    GstCaps* caps = gst_sample_get_caps(sample); // Lấy caps
    GstStructure* s = gst_caps_get_structure(caps, 0); // Lấy cấu trúc caps

    int width = 0, height = 0; // Khai báo kích thước
    gst_structure_get_int(s, "width", &width); // Lấy width
    gst_structure_get_int(s, "height", &height); // Lấy height

    GstMapInfo map; // Struct để map buffer
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) { // Map buffer
        std::cerr << "[ERROR] Không map buffer\n"; // Báo lỗi
        gst_sample_unref(sample); // Giải phóng sample
        return {}; // Trả về rỗng
    }

    size_t expectedSize = width * height * 3 / 2; // Tính kích thước NV12
    if (map.size < expectedSize) { // Nếu nhỏ hơn
        std::cerr << "[WARN] Kích thước buffer nhỏ hơn NV12 chuẩn\n"; // Cảnh báo
    }

    cv::Mat yuv(height + height / 2, width, CV_8UC1, (uchar*)map.data); // Tạo ảnh YUV
    cv::Mat bgr; // Khởi tạo ảnh BGR
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV12); // Chuyển đổi YUV → BGR

    gst_buffer_unmap(buffer, &map); // Bỏ map
    gst_sample_unref(sample); // Giải phóng sample

    return bgr; // Trả về ảnh BGR
}

void RTSPReader::readThreadFunc() {
    int frameCount = 0; // Đếm frame
    auto startTime = std::chrono::steady_clock::now(); // Ghi thời gian bắt đầu

    while (running) { // Khi còn chạy
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink)); // Lấy sample
        if (!sample) continue; // Nếu không có thì tiếp tục

        GstCaps *caps = gst_sample_get_caps(sample); // Lấy metadata
        GstStructure *s = gst_caps_get_structure(caps, 0); // Trích xuất

        int width = 0, height = 0; // Kích thước ảnh
        const gchar* format = gst_structure_get_string(s, "format"); // Định dạng
        gst_structure_get_int(s, "width", &width); // Chiều rộng
        gst_structure_get_int(s, "height", &height); // Chiều cao

        GstBuffer* buffer = gst_sample_get_buffer(sample); // Lấy buffer
        GstClockTime pts = GST_BUFFER_PTS(buffer); // Lấy PTS (timestamp)
        double time_sec = (pts != GST_CLOCK_TIME_NONE) ? (pts / 1e9) : -1; // Chuyển sang giây

        std::cout << "[Frame] "  // Log thông tin ảnh
                  << "Định dạng ảnh=" << format 
                  << " | Độ phân giải =" << width << "x" << height
                  << " | PTS=" << time_sec << "s"
                  << std::endl;

        cv::Mat frame = gstSampleToMat(sample); // Chuyển sample sang ảnh

        {
            std::lock_guard<std::mutex> lock(frameMutex); // Khoá thread an toàn
            currentFrame = std::move(frame); // Lưu frame hiện tại
            frame.release(); // Giải phóng sớm
        }

        frameCount++; // Tăng đếm
        auto now = std::chrono::steady_clock::now(); // Giờ hiện tại
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count(); // Thời gian đã qua
        if (elapsed >= 1) { // Mỗi giây
            std::cout << "[FPS] " << frameCount << " khung hình/giây" << std::endl; // In FPS
            frameCount = 0; // Reset
            startTime = now; // Cập nhật lại thời gian
        }
    }
}

void RTSPReader::readStream() {
    running = true; // Đánh dấu đang chạy
    readThread = std::thread(&RTSPReader::readThreadFunc, this); // Tạo luồng xử lý ảnh

    while (running) { // Vòng lặp hiển thị
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(frameMutex); // Khoá thread
            if (currentFrame.empty()) continue; // Nếu rỗng thì skip
            frame = currentFrame; // Lấy frame hiện tại
        }

        cv::imshow("RTSP Stream", frame); // Tùy chọn hiển thị
        if (cv::waitKey(1) == 27) { // Nhấn ESC để thoát
            stop(); // Dừng
            break; // Thoát vòng lặp
        }
    }
}

bool RTSPReader::getCurrentFrame(cv::Mat& outFrame) {
    std::lock_guard<std::mutex> lock(frameMutex); // Thread-safe
    if (currentFrame.empty()) return false; // Nếu rỗng
    outFrame = currentFrame.clone(); // Trả ra bản sao ảnh hiện tại
    return true; // Thành công
}
