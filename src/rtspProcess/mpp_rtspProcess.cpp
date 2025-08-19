#include "mpp_rtspProcess.h"           // Header khai báo class rtsp_reader

//==================== CONSTRUCTOR ====================//
// Hàm khởi tạo class rtsp_reader.
// Thiết lập URL RTSP, khởi tạo các biến pipeline và thông số FPS.
rtsp_reader::rtsp_reader(const std::string& url)
    : rtsp_url_(url)
{
    gst_init(nullptr, nullptr);
    frame_count_ = 0;
    last_fps_time_ = std::chrono::steady_clock::now();
    pipeline_upload_ = nullptr;
    appsrc_upload_   = nullptr;
    capsfilter_upload_ = nullptr;
    enc_upload_      = nullptr;
    sink_upload_     = nullptr;
}

//==================== DESTRUCTOR ====================//
// Hàm hủy class rtsp_reader.
// Đảm bảo giải phóng pipeline và các tài nguyên GStreamer khi đối tượng bị hủy.
rtsp_reader::~rtsp_reader() {
    if (pipeline_) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    if (pipeline_upload_) {
        gst_element_set_state(pipeline_upload_, GST_STATE_NULL);
        gst_object_unref(pipeline_upload_);
    }
    pipeline_upload_ = nullptr;
    appsrc_upload_ = nullptr;
    capsfilter_upload_ = nullptr;
    enc_upload_ = nullptr;
    sink_upload_ = nullptr;
}

//gst-launch-1.0 rtspsrc location=rtsp://103.147.186.175:8554/9L02DA3PAJ39B2F protocols=tcp latency=100 \
   ! rtph264depay ! h264parse ! mppvideodec ! kmssink sync=false

// gst-launch-1.0 rtspsrc location=rtsp://103.147.186.175:8554/9L02DA3PAJ39B2F protocols=tcp latency=30 \
  ! rtph264depay \
  ! h264parse \
  ! mppvideodec \
  ! videoconvert \
  ! fpsdisplaysink video-sink=kmssink text-overlay=true sync=false

//==================== INIT PIPELINE ====================//
// Hàm khởi tạo pipeline GStreamer để nhận luồng RTSP, giải mã H264 và lấy frame NV12 DMABuf.
// Tạo các element, cấu hình, gắn callback và nối pipeline.
// Trả về true nếu thành công, false nếu lỗi.
bool rtsp_reader::init() {
    // 1️⃣ Tạo pipeline và các element
    pipeline_ = gst_pipeline_new("rtsp-h264-pipeline"); // Pipeline tổng
    source_   = gst_element_factory_make("rtspsrc", "source");       // Nguồn RTSP
    depay_    = gst_element_factory_make("rtph264depay", "depay");   // Gỡ RTP -> H264
    parse_    = gst_element_factory_make("h264parse", "parse");      // Parse H264 stream
    queue_    = gst_element_factory_make("queue", "queue");          // Bộ đệm queue
    decoder_  = gst_element_factory_make("mppvideodec","decoder");  
    sink_     = gst_element_factory_make("appsink", "appsink");      // Appsink nhận dữ liệu

    if (!pipeline_ || !source_ || !depay_ || !parse_ || !queue_ || !decoder_ || !sink_) {
        std::cerr << "[ERROR] Không thể tạo element GStreamer." << std::endl;
        return false;    // Nếu thiếu bất kỳ element nào thì thất bại
    }

    // 2️⃣ Cấu hình các element
    g_object_set(G_OBJECT(source_), "location", rtsp_url_.c_str(), nullptr); // Set RTSP URL
    g_object_set(G_OBJECT(source_), "latency", 200, nullptr);                // Buffer latency 200ms
    g_object_set(G_OBJECT(source_), "protocols", 4, nullptr);                // 4 = TCP only
    g_object_set(G_OBJECT(parse_), "config-interval", 1, nullptr);           // Gửi SPS/PPS định kỳ
    GstCaps *caps = gst_caps_from_string("video/x-raw, format=NV12, memory:DMABuf");
    //Cấu hình appsink để zerocopy NV12 DMABUF
    gst_app_sink_set_caps(GST_APP_SINK(sink_), caps);
    gst_caps_unref(caps);
    g_object_set(G_OBJECT(sink_), "emit-signals", TRUE,         //emit-signals=TRUE để bắt callback new-sample
                                  "sync", FALSE,                //sync=FALSE để không block theo clock
                                  "drop", FALSE,                //tránh rớt frame khi chậm
                                  "max-buffers", 5, nullptr);   //giới hạn queue trong appsink

    // 3️⃣ Gắn callback
    g_signal_connect(sink_, "new-sample", G_CALLBACK(rtsp_reader::onNewSample), this);
    // Khi appsink có sample mới, gọi onNewSample
    g_signal_connect(source_, "pad-added", G_CALLBACK(rtsp_reader::onPadAdded), depay_);
    // Khi rtspsrc tạo pad động, nối nó với depay

    // 4️⃣ Thêm các element vào pipeline
    gst_bin_add_many(GST_BIN(pipeline_), source_, depay_, parse_, queue_, decoder_, sink_, nullptr);

    // Nối phần cố định (không bao gồm rtspsrc vì pad động)
    if (!gst_element_link_many(depay_, parse_, queue_, decoder_, sink_, nullptr)) {
        std::cerr << "[ERROR] Lỗi nối phần tử pipeline." << std::endl;
        return false;
    }

    return true; // Thành công
}

//==================== RUN PIPELINE ====================//
// Hàm chạy pipeline GStreamer (set PLAYING), lắng nghe các sự kiện lỗi/EOS.
// Khi có lỗi hoặc kết thúc stream sẽ dừng pipeline.
void rtsp_reader::run() {
    // 1️⃣ Bắt đầu pipeline
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);  // Chuyển sang PLAYING
    std::cout << "[INFO] Pipeline đang chạy..." << std::endl;

    GstBus* bus = gst_element_get_bus(pipeline_);  // Lấy message bus để nhận sự kiện
    
    
    bool running = true;

    // 2️⃣ Vòng lặp chính để nhận thông báo từ GStreamer
    while (running) {
        GstMessage* msg = gst_bus_timed_pop_filtered(
            bus,
            100 * GST_MSECOND,                               // Timeout 100ms thay vì chờ vô hạn
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS) // Chỉ quan tâm Error hoặc EOS
        );

        if (msg != nullptr) {
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {                    // Nếu có lỗi
                    GError* err;
                    gchar* debug;
                    gst_message_parse_error(msg, &err, &debug); // Parse nội dung lỗi
                    std::cerr << "[ERROR] " << err->message << std::endl;
                    g_error_free(err);
                    g_free(debug);
                    running = false;                         // Thoát vòng lặp
                    break;
                }
                case GST_MESSAGE_EOS:                        // Nếu stream kết thúc
                    std::cout << "[INFO] Kết thúc stream." << std::endl;
                    running = false;
                    break;
                default:
                    break;
            }
            gst_message_unref(msg); // Giải phóng message
        }
    }
    // Dừng pipeline khi thoát vòng lặp
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(bus);
}

//==================== CALLBACK: XỬ LÝ BUFFER ====================//
// Callback static được gọi khi appsink có sample mới (frame mới).
// Lấy thông tin frame, gọi callback xử lý frame (nếu có), đo FPS và thời gian xử lý.
GstFlowReturn rtsp_reader::onNewSample(GstAppSink* appsink, gpointer user_data) {
    rtsp_reader* self = reinterpret_cast<rtsp_reader*>(user_data);
    auto frame_start = std::chrono::steady_clock::now();
    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample) {
        std::cerr << "[ERROR] Sample null" << std::endl;
        return GST_FLOW_OK;
}
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        std::cerr << "[ERROR] Buffer null" << std::endl;
        gst_sample_unref(sample);
        return GST_FLOW_OK;
}
    GstMemory* mem = gst_buffer_peek_memory(buffer, 0);
    int width = 0, height = 0;
    if (GstCaps* caps = gst_sample_get_caps(sample)) {
        GstStructure* s = gst_caps_get_structure(caps, 0);
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);
}
    bool used_cpu_copy = false;
    if (mem && gst_is_dmabuf_memory(mem)) {
        int fd = gst_dmabuf_memory_get_fd(mem);
        std::cout << "[INFO] Nhận NV12 DMABUF fd=" << fd << " size=" << width << "x" << height << std::endl;
        if (self->frameCallback_) self->frameCallback_(fd, width, height);
        
    } else { // CPU buffer
        GstMapInfo info;
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            std::cout << "[INFO] Nhận NV12 frame size=" << info.size << " bytes (CPU copy)" << std::endl;
            
            gst_buffer_unmap(buffer, &info);
            used_cpu_copy = true;
    }
}
    // FPS logic
    self->frame_count_++;
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - self->last_fps_time_).count() >= 1) {
        std::cout << "[INFO] FPS = " << self->frame_count_ << (used_cpu_copy ? " (CPU path)" : "") << std::endl;
        self->frame_count_ = 0;
        self->last_fps_time_ = now;
}
    // Processing time warn
    auto frame_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - frame_start).count();
    if (frame_elapsed > 30) {
        std::cout << "[WARN] Xử lý frame mất " << frame_elapsed << " ms" << std::endl;
}
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

//==================== CALLBACK: XỬ LÝ PAD ĐỘNG ====================//
// Callback static được gọi khi rtspsrc tạo pad động (luồng video mới).
// Nối pad động từ rtspsrc vào depay để pipeline hoạt động đúng.
void rtsp_reader::onPadAdded(GstElement* src, GstPad* new_pad, gpointer data) {
    GstElement* depay = static_cast<GstElement*>(data);       // Lấy depay từ user_data
    GstPad* sink_pad = gst_element_get_static_pad(depay, "sink"); // Lấy sink pad của depay

    if (gst_pad_is_linked(sink_pad)) {                        // Nếu đã được nối
        gst_object_unref(sink_pad);                           // Giải phóng pad
        return;                                               // Không làm gì nữa
}

    // Nối pad động từ rtspsrc vào depay
    if (gst_pad_link(new_pad, sink_pad) != GST_PAD_LINK_OK)
        std::cerr << "[ERROR] Không thể nối pad rtspsrc → depay" << std::endl;
    else
        std::cout << "[INFO] Pad RTSP nối thành công!" << std::endl;

    gst_object_unref(sink_pad);                               // Giải phóng sink pad
}
