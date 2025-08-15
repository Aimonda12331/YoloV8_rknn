#ifndef MPP_RTSP_READER_H
#define MPP_RTSP_READER_H

#include <iostream>               // Dùng cho std::cout và std::cerr
#include <string.h>
#include <chrono>
#include <functional>
#include <cstring>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/allocators.h>
#include <gst/allocators/gstdmabuf.h>

class rtsp_reader{
    public:
        using FrameCallback = std::function<void(int dma_fd, int width, int height)>;

        explicit rtsp_reader(const std::string& url); //hàm khởi tạo nhận url rtsp
        ~rtsp_reader();                               //hàm dọn dẹp tài nguyên
        rtsp_reader(const rtsp_reader&) = delete;      // Không cho copy
        rtsp_reader& operator=(const rtsp_reader&) = delete;
        
        bool init();                                  //hàm khởi tạo cho pipeline
        void run();                                   //hàm chạy pipeline
        
        // Cho phép set callback từ bên ngoài
        void setFrameCallback(FrameCallback cb) { frameCallback_ = cb; }

    private:
        static GstFlowReturn onNewSample(GstAppSink* appsink, gpointer user_data); // Callback static khi có sample mới
        static void onPadAdded(GstElement* src, GstPad* new_pad, gpointer data);   // Callback static khi rtspsrc tạo pad động
 
        std::string rtsp_url_;            //URL của thằng RTSP
        GstElement* pipeline_   = nullptr; // Pipeline tổng
        GstElement* source_     = nullptr; // rtspsrc
        GstElement* depay_      = nullptr; // rtph264depay
        GstElement* parse_      = nullptr; // h264parse
        GstElement* queue_      = nullptr; // queue
        GstElement* decoder_    = nullptr; // mppdecoder
        GstElement* sink_       = nullptr; // appsink

        FrameCallback frameCallback_;  // callback từ bên ngoài

        size_t frame_count_ = 0;
        std::chrono::steady_clock::time_point last_fps_time_;

        // ⚠️⚠️⚠️ TODO: TẠM ẨN UPLOAD PIPELINE MEMBERS - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
        // Pipeline upload
        GstElement* pipeline_upload_ = nullptr;
        GstElement* appsrc_upload_ = nullptr;
        GstElement* capsfilter_upload_ = nullptr;  // Thêm capsfilter member
        GstElement* enc_upload_ = nullptr;
        GstElement* sink_upload_ = nullptr;
};

#endif