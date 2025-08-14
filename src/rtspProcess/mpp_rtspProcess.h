#ifndef MPP_RTSP_READER_H
#define MPP_RTSP_READER_H

#include <iostream>
#include <string.h>
#include <chrono>
#include <functional>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/allocators/allocators.h>
#include <gst/allocators/gstdmabuf.h>


class rtsp_reader{
    public:
        using FrameCallback = std::function<void(int dma_fd, int width, int height)>;
        
        // ⚠️ TODO: TẠM ẨN UPLOAD CALLBACK TYPE - ENABLE LẠI KHI CẦN ⚠️
        // using UploadCallback = std::function<void(int dma_fd, int width, int height)>;

        explicit rtsp_reader(const std::string& url); //hàm khởi tạo nhận url rtsp
        ~rtsp_reader();                               //hàm dọn dẹp tài nguyên
        rtsp_reader(const rtsp_reader&) = delete;      // Không cho copy
        rtsp_reader& operator=(const rtsp_reader&) = delete;
        
        bool init();                                  //hàm khởi tạo cho pipeline
        void run();                                   //hàm chạy pipeline
        
        // ⚠️⚠️⚠️ TODO: TẠM ẨN UPLOAD FUNCTIONS - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
        // bool init_pipeline_upload(const std::string& upload_url, int width, int height, int framerate);
        // void push_frame_to_upload(int dma_fd, int width, int height);
        
        // Cho phép set callback từ bên ngoài
        void setFrameCallback(FrameCallback cb) { frameCallback_ = cb; }
        // ⚠️ TODO: TẠM ẨN UPLOAD CALLBACK - ENABLE LẠI KHI CẦN ⚠️
        // void setUploadCallback(UploadCallback cb) { uploadCallback_ = cb; }

    private:
        static GstFlowReturn onNewSample(GstAppSink* appsink, gpointer user_data); // Callback static khi có sample mới
        static void onPadAdded(GstElement* src, GstPad* new_pad, gpointer data);   // Callback static khi rtspsrc tạo pad động
        
        // ⚠️ TODO: TẠM ẨN UPLOAD CALLBACK - ENABLE LẠI KHI CẦN ⚠️
        // static void onUploadStateChanged(GObject* object, GParamSpec* pspec, gpointer user_data); // Monitor upload state
 
        std::string rtsp_url_;            //URL của thằng RTSP
        GstElement* pipeline_   = nullptr; // Pipeline tổng
        GstElement* source_     = nullptr; // rtspsrc
        GstElement* depay_      = nullptr; // rtph264depay
        GstElement* parse_      = nullptr; // h264parse
        GstElement* queue_      = nullptr; // queue
        GstElement* decoder_    = nullptr; // mppdecoder
        GstElement* sink_       = nullptr; // appsink

        FrameCallback frameCallback_;  // callback từ bên ngoài
        
        // ⚠️ TODO: TẠM ẨN UPLOAD CALLBACK MEMBER - ENABLE LẠI KHI CẦN ⚠️
        // UploadCallback uploadCallback_;  // callback for upload pipeline

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