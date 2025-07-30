<!-- <!-- ===GHI CHÚ===
Cài gstreamer libav (để có avdec_h264)
Chạy lệnh sau để cài plugin giải mã phần mềm:

sudo apt update
sudo apt install gstreamer1.0-libav

===TIẾN TRÌNH===
Bước 1. Khởi tạo camera (RTSPReader)
        Khởi tạo mô hình yolov8.rknn (Yolo8InitModel) 
Bước 2. Preprocess
    Mục tiêu: 
        Lấy hình từ RTSPReader
        Resize ảnh về kích thước đầu vào model (ví dụ 640x640)
        Chuyển màu BGR ➝ RGB
        (Tùy do_preprocess): Normalize và hoán vị HWC → CHW
 -->


ảnh lấy từ hệ thống có độ phân giải 1600x1200 dẫn đến CPU cao nên ép ảnh lại thành 640x480 --> áp dụng người
#include "rtspReader.h"
#include <iostream>
#include <chrono>

RTSPReader::RTSPReader(const std::string& rtsp_URL)
    : url(rtsp_URL), running(false), pipeline(nullptr), appsink(nullptr) {
    gst_init(nullptr, nullptr);
}

RTSPReader::~RTSPReader() {
    stop();
}

void RTSPReader::stop() {
    running = false;
    if (readThread.joinable()) readThread.join();

    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        pipeline = nullptr;
    }

    if (appsink) {
    gst_object_unref(appsink);
    appsink = nullptr;
}

    cv::destroyAllWindows();
}

bool RTSPReader::open() {
    std::string pipelineStr =
        "rtspsrc location=" + url + " latency=200 protocols=tcp ! "
        "queue max-size-buffers=100 leaky=downstream ! " 
        "rtph264depay ! " 
        "queue ! " 
        "h264parse config-interval=1 ! "
        "queue max-size-buffers=100 leaky=downstream ! " //leaky=downstream
        "mppvideodec ! "
        "queue max-size-buffers=100 leaky=downstream ! "
        "videoscale method=1 ! " 
        "video/x-raw, format=NV12, width=640, height=480, memory=DMABuf ! "
        "appsink name=mysink max-buffers=1 drop=true sync=false";

    GError* error = nullptr;
    pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (!pipeline) {
        std::cerr << "[ERROR] Tạo pipeline thất bại: " << error->message << std::endl;
        g_error_free(error);
        return false;
    }

    appsink = gst_bin_get_by_name(GST_BIN(pipeline), "mysink");
    if (!appsink) {
        std::cerr << "[ERROR] Không tìm thấy appsink.\n";
        return false;
    }

    gst_app_sink_set_emit_signals(GST_APP_SINK(appsink), false);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), true);
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    std::cout << "✅ RTSP kết nối thành công. DMA-BUF đang hoạt động.\n";
    return true;
}

// Dành cho AI xử lý: trả về DMA fd (zero-copy)
int RTSPReader::getCurrentDmaFd() {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) return -1;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        gst_sample_unref(sample);
        return -1;
    }

    GstMemory* mem = gst_buffer_peek_memory(buffer, 0);
    if (!gst_is_dmabuf_memory(mem)) {
        std::cerr << "[ERROR] Memory không phải DMABuf.\n";
        gst_sample_unref(sample);
        return -1;
    }

    int fd = gst_dmabuf_memory_get_fd(mem);
    gst_sample_unref(sample);
    return fd;
}

// Dành cho hiển thị / debug
cv::Mat RTSPReader::gstSampleToMat(GstSample* sample) {

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    GstStructure* s = gst_caps_get_structure(caps, 0);

    int width = 0, height = 0;
    //int width, height;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);

    GstMapInfo map;
    // gst_buffer_map(buffer, &map, GST_MAP_READ);
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        std::cerr << "[ERROR] Không map buffer\n";
        gst_sample_unref(sample);
        return {};
    }

    size_t expectedSize = width * height * 3 / 2;
    if (map.size < expectedSize) {
        std::cerr << "[WARN] Kích thước buffer nhỏ hơn NV12 chuẩn\n";
    }

    cv::Mat yuv(height + height / 2, width, CV_8UC1, (uchar*)map.data);
    cv::Mat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV2BGR_NV12);

      // Giải phóng buffer và sample
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    return bgr;
}

// Luồng xử lý nhận ảnh
void RTSPReader::readThreadFunc() {
    int frameCount = 0;
    auto startTime = std::chrono::steady_clock::now();

    while (running) {
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
        if (!sample) continue;

        // 👉 Lấy thông tin trước khi chuyển thành Mat
        GstCaps *caps = gst_sample_get_caps(sample);
        GstStructure *s = gst_caps_get_structure(caps, 0);

        int width = 0, height = 0;
        const gchar* format = gst_structure_get_string(s, "format");
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        GstClockTime pts = GST_BUFFER_PTS(buffer);
        double time_sec = (pts != GST_CLOCK_TIME_NONE) ? (pts / 1e9) : -1;

        std::cout << "[Frame] " 
                  << "Định dạng ảnh=" << format 
                  << " | Độ phân giải =" << width << "x" << height
                  << " | PTS=" << time_sec << "s"
                  << std::endl;

        // 👉 Tiếp tục chuyển sample thành Mat
        cv::Mat frame = gstSampleToMat(sample);

        {
            std::lock_guard<std::mutex> lock(frameMutex);
            // currentFrame = frame.clone();  → bỏ .clone() nếu không dùng song song
                currentFrame = std::move(frame);
                frame.release();  // Giải phóng sớm
        }

        frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count();
        if (elapsed >= 1) {
            std::cout << "[FPS] " << frameCount << " khung hình/giây" << std::endl;
            frameCount = 0;
            startTime = now;
        }
    }
}

// Hiển thị stream
void RTSPReader::readStream() {
    running = true;
    readThread = std::thread(&RTSPReader::readThreadFunc, this);

    while (running) {
        cv::Mat frame;
        {
            std::lock_guard<std::mutex> lock(frameMutex);
            if (currentFrame.empty()) continue;
            //frame = currentFrame.clone();
            frame = currentFrame;
        }

        cv::imshow("RTSP Stream", frame);
        if (cv::waitKey(1) == 27) {
            stop();
            break;
        }
    }
}

// Lấy ảnh từ thread
bool RTSPReader::getCurrentFrame(cv::Mat& outFrame) {
    std::lock_guard<std::mutex> lock(frameMutex);
    if (currentFrame.empty()) return false;

    outFrame = currentFrame.clone(); // đảm bảo dữ liệu an toàn khi chia sẻ
    return true;
}

max-buffer= không thấy tăng ram
