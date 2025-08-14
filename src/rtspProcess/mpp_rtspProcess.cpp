#include "mpp_rtspProcess.h"           // Header khai báo class rtsp_reader
#include <iostream>               // Dùng cho std::cout và std::cerr
#include <gst/app/gstappsrc.h>
#include <gst/allocators/gstdmabuf.h>
#include <cstring>

//==================== CONSTRUCTOR ====================//
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

//==================== INIT PIPELINE PHÁT RTSP ====================//
// ⚠️⚠️⚠️ TODO: ĐANG TẠM ẨN UPLOAD RTSP - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
// XÓA hàm init_publish cũ (nếu còn) và thay bằng init_pipeline_upload
/*
bool rtsp_reader::init_pipeline_upload(const std::string& upload_url, int width, int height, int framerate) {
    if (pipeline_upload_) return true; // đã khởi tạo
    pipeline_upload_ = gst_pipeline_new("rtsp-upload-pipeline");
    appsrc_upload_   = gst_element_factory_make("appsrc", "upload_src");
    capsfilter_upload_ = gst_element_factory_make("capsfilter", "caps_filter");
    enc_upload_      = gst_element_factory_make("mpph264enc", "upload_enc");
    GstElement* h264parse = gst_element_factory_make("h264parse", "h264parse");
    GstElement* rtppay = gst_element_factory_make("rtph264pay", "rtppay");
    sink_upload_ = gst_element_factory_make("rtspclientsink", "upload_sink");
    // sink_upload_ = gst_element_factory_make("fakesink", "upload_sink");
    if (!pipeline_upload_ || !appsrc_upload_ || !capsfilter_upload_ || !enc_upload_ || !h264parse || !rtppay || !sink_upload_) {
        std::cerr << "[ERROR] Không thể tạo pipeline upload." << std::endl;
        return false;
    }
    // Cấu hình appsrc
    g_object_set(G_OBJECT(appsrc_upload_),
                 "is-live", TRUE,
                 "format", GST_FORMAT_TIME,
                 "do-timestamp", TRUE,
                 "block", FALSE,  // Không block để tránh treo callback
                 "max-buffers", 3,  // Giới hạn buffer queue
                 nullptr);
    // Encoder cấu hình cho RTSP streaming
    g_object_set(G_OBJECT(enc_upload_),
                 "bps", 2000000,         // Giảm bitrate xuống 2Mbps
                 "bps-max", 4000000,     // Max bitrate 4Mbps  
                 "rc-mode", 1,           // CBR (Constant Bitrate)
                 "gop", 50,              // GOP size 50 frames (2s với 25fps)
                 "profile", 100,         // H.264 High Profile
                 "level", 31,            // Level 3.1
                 nullptr);
    // Cấu hình h264parse cho RTSP
    g_object_set(G_OBJECT(h264parse),
                 "config-interval", 1,    // Gửi SPS/PPS mỗi GOP
                 nullptr);
                 
    // Cấu hình RTP payloader
    g_object_set(G_OBJECT(rtppay),
                 "config-interval", 1,    // Gửi SPS/PPS trong RTP
                 "pt", 96,                // Payload type
                 nullptr);
    // Cấu hình rtspclientsink
    g_object_set(G_OBJECT(sink_upload_), 
                 "location", upload_url.c_str(),
                 "latency", 2000,           // 2s latency buffer
                 "protocols", 4,            // TCP only
                 "retry", 5,                // Retry 5 times
                 "timeout", 20,             // 20s timeout
                 nullptr);
    
    std::cout << "[INFO] Cấu hình RTSP upload tới: " << upload_url << std::endl;
    
    // Cấu hình fakesink để log (chỉ khi dùng fakesink)
    // g_object_set(G_OBJECT(sink_upload_), "verbose", TRUE, nullptr);
    
    char caps_str[160];
    // Dynamic caps based on parameters  
    snprintf(caps_str, sizeof(caps_str), "video/x-raw,format=NV12,width=%d,height=%d,framerate=%d/1", width, height, framerate);
    
    GstCaps* input_caps = gst_caps_from_string(caps_str);
    g_object_set(G_OBJECT(appsrc_upload_), "caps", input_caps, nullptr);
    gst_caps_unref(input_caps);
    
    // Capsfilter để đảm bảo format cho encoder
    GstCaps* filter_caps = gst_caps_from_string("video/x-raw,format=NV12");
    g_object_set(G_OBJECT(capsfilter_upload_), "caps", filter_caps, nullptr);
    gst_caps_unref(filter_caps);
    
    gst_bin_add_many(GST_BIN(pipeline_upload_), appsrc_upload_, capsfilter_upload_, enc_upload_, h264parse, rtppay, sink_upload_, nullptr);
    if (!gst_element_link_many(appsrc_upload_, capsfilter_upload_, enc_upload_, h264parse, rtppay, sink_upload_, nullptr)) {
        std::cerr << "[ERROR] Không thể link pipeline upload." << std::endl;
        return false;
    }
    if (gst_element_set_state(pipeline_upload_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[ERROR] Không thể PLAY pipeline upload" << std::endl;
        return false;
    }
    
    // Kiểm tra error trên bus của pipeline upload
    GstBus* upload_bus = gst_element_get_bus(pipeline_upload_);
    GstMessage* msg = gst_bus_pop_filtered(upload_bus, GST_MESSAGE_ERROR);
    if (msg) {
        GError* err;
        gchar* debug;
        gst_message_parse_error(msg, &err, &debug);
        std::cerr << "[ERROR] Upload pipeline error: " << err->message << std::endl;
        if (debug) std::cerr << "[DEBUG] " << debug << std::endl;
        g_error_free(err);
        g_free(debug);
        gst_message_unref(msg);
        gst_object_unref(upload_bus);
        return false;
    }
    
    // Thêm callback để monitor trạng thái upload pipeline
    g_signal_connect(sink_upload_, "notify::state", G_CALLBACK(rtsp_reader::onUploadStateChanged), this);
    
    gst_object_unref(upload_bus);
    
    std::cout << "[INFO] Đã khởi tạo pipeline upload với rtspclientsink" << std::endl;
    return true;
}
*/

//==================== ĐẨY FRAME LÊN RTSP SERVER ====================//
// ⚠️⚠️⚠️ TODO: ĐANG TẠM ẨN UPLOAD RTSP - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
/*
void rtsp_reader::push_frame_to_upload(int dma_fd, int width, int height) {
    // Nếu là DMABUF, tạo buffer từ fd và push vào appsrc_upload_
    if (!appsrc_upload_) {
        std::cerr << "[ERROR] appsrc_upload_ chưa được khởi tạo!" << std::endl;
        return;
    }
    if (dma_fd < 0) {
        std::cerr << "[WARN] Không hỗ trợ push CPU buffer lên upload pipeline!" << std::endl;
        return;
    }
    // Log trước khi tạo buffer
    std::cout << "[DEBUG] push_frame_to_upload: dma_fd=" << dma_fd << ", w=" << width << ", h=" << height << std::endl;
    GstBuffer* buffer = gst_buffer_new();
    if (!buffer) {
        std::cerr << "[ERROR] Không tạo được GstBuffer!" << std::endl;
        return;
    }
    gsize size = width * height * 3 / 2;
    GstAllocator *allocator = gst_dmabuf_allocator_new();
    GstMemory* mem = gst_dmabuf_allocator_alloc(allocator, dma_fd, size);
    gst_object_unref(allocator);
    if (!mem) {
        std::cerr << "[ERROR] Không tạo được GstMemory từ DMABuf fd!" << std::endl;
        gst_buffer_unref(buffer);
        return;
    }
    gst_buffer_append_memory(buffer, mem);
    
    // Thêm timestamp cho buffer
    static GstClockTime timestamp = 0;
    GST_BUFFER_PTS(buffer) = timestamp;
    GST_BUFFER_DTS(buffer) = timestamp;
    GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 25); // 25 FPS
    timestamp += GST_BUFFER_DURATION(buffer);
    
    // Log trước khi push buffer
    std::cout << "[DEBUG] Pushing buffer to appsrc_upload_..." << std::endl;
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_upload_), buffer);
    if (ret != GST_FLOW_OK) {
        std::cerr << "[ERROR] Push buffer lên appsrc_upload_ thất bại! ret=" << ret << std::endl;
        if (ret == GST_FLOW_FLUSHING) {
            std::cerr << "[INFO] Pipeline upload đang flushing/stopping" << std::endl;
        } else if (ret == GST_FLOW_EOS) {
            std::cerr << "[INFO] Pipeline upload đã EOS" << std::endl;
        }
    } else {
        std::cout << "[DEBUG] Push buffer thành công." << std::endl;
    }
}
*/

//==================== RUN PIPELINE ====================//
void rtsp_reader::run() {
    // 1️⃣ Bắt đầu pipeline
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);  // Chuyển sang PLAYING
    std::cout << "[INFO] Pipeline đang chạy..." << std::endl;

    GstBus* bus = gst_element_get_bus(pipeline_);  // Lấy message bus để nhận sự kiện
    
    // ⚠️ TODO: TẠM ẨN UPLOAD BUS - ENABLE LẠI KHI CẦN ⚠️
    // GstBus* upload_bus = nullptr;
    // if (pipeline_upload_) {
    //     upload_bus = gst_element_get_bus(pipeline_upload_);
    // }
    
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
        
        // ⚠️ TODO: TẠM ẨN UPLOAD BUS MONITORING - ENABLE LẠI KHI CẦN ⚠️
        // Kiểm tra upload pipeline messages
        /*
        if (upload_bus) {
            GstMessage* upload_msg = gst_bus_pop(upload_bus);
            if (upload_msg) {
                switch (GST_MESSAGE_TYPE(upload_msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError* err;
                        gchar* debug;
                        gst_message_parse_error(upload_msg, &err, &debug);
                        std::cerr << "[UPLOAD ERROR] " << err->message << std::endl;
                        if (debug) std::cerr << "[UPLOAD DEBUG] " << debug << std::endl;
                        g_error_free(err);
                        g_free(debug);
                        break;
                    }
                    case GST_MESSAGE_WARNING: {
                        GError* warn;
                        gchar* debug;
                        gst_message_parse_warning(upload_msg, &warn, &debug);
                        std::cout << "[UPLOAD WARN] " << warn->message << std::endl;
                        if (debug) std::cout << "[UPLOAD DEBUG] " << debug << std::endl;
                        g_error_free(warn);
                        g_free(debug);
                        break;
                    }
                    case GST_MESSAGE_STATE_CHANGED: {
                        if (GST_MESSAGE_SRC(upload_msg) == GST_OBJECT(sink_upload_)) {
                            GstState old_state, new_state;
                            gst_message_parse_state_changed(upload_msg, &old_state, &new_state, nullptr);
                            std::cout << "[UPLOAD] State: " << gst_element_state_get_name(old_state) 
                                     << " → " << gst_element_state_get_name(new_state) << std::endl;
                        }
                        break;
                    }
                    default:
                        break;
                }
                gst_message_unref(upload_msg);
            }
        }
        */
    }

    // Dừng pipeline khi thoát vòng lặp
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(bus);
    
    // ⚠️ TODO: TẠM ẨN UPLOAD BUS CLEANUP - ENABLE LẠI KHI CẦN ⚠️
    // if (upload_bus) gst_object_unref(upload_bus);
}

//==================== CALLBACK: XỬ LÝ BUFFER ====================//
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
        
        // ⚠️ TODO: TẠM ẨN UPLOAD RTSP - ENABLE LẠI KHI CẦN ⚠️
        // if (self->appsrc_upload_) {
        //     self->push_frame_to_upload(fd, width, height);
        // }
    } else { // CPU buffer
        GstMapInfo info;
        if (gst_buffer_map(buffer, &info, GST_MAP_READ)) {
            std::cout << "[INFO] Nhận NV12 frame size=" << info.size << " bytes (CPU copy)" << std::endl;
            
            // ⚠️ TODO: TẠM ẨN UPLOAD RTSP - ENABLE LẠI KHI CẦN ⚠️  
            // if (self->appsrc_upload_) self->push_frame_to_upload(-1, width, height); // Pass -1 for dma_fd when using CPU copy
            
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

//==================== CALLBACK: MONITOR UPLOAD STATE ====================//
// ⚠️⚠️⚠️ TODO: ĐANG TẠM ẨN UPLOAD CALLBACK - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
/*
void rtsp_reader::onUploadStateChanged(GObject* object, GParamSpec* pspec, gpointer user_data) {
    rtsp_reader* self = reinterpret_cast<rtsp_reader*>(user_data);
    GstElement* sink = GST_ELEMENT(object);
    
    GstState state, pending;
    GstStateChangeReturn ret = gst_element_get_state(sink, &state, &pending, 0);
    
    const char* state_name = gst_element_state_get_name(state);
    std::cout << "[UPLOAD] Sink state changed to: " << state_name << std::endl;
    
    if (state == GST_STATE_NULL || state == GST_STATE_READY) {
        std::cout << "[WARN] Upload sink không ở trạng thái PLAYING - có thể kết nối thất bại!" << std::endl;
    }
}
*/


