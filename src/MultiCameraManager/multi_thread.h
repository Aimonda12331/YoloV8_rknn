#ifndef MULTI_THREAD_H
#define MULTI_THREAD_H

    #include <iostream>
    #include <string>
    #include <vector>
    #include <memory>
    #include <thread>
    #include <atomic>
    
    #include <nv12_converter_all_formats/nv12_converter_all_formats.h>
    #include <rtspProcess/mpp_rtspProcess.h>
    #include <CameraContext.h>

// Forward declaration hoặc định nghĩa CameraContext nếu chưa có header riêng
struct CameraContext;
/** 
* @brief quản lý đa luồng cho nhiều camera RTSP 
*   Mỗi camera chạy 1 thread riêng, nhận fremme NV12 và chuyển đổi sang định dạng mong muốn
**/

class MultiCameraManager {
public:
    MultiCameraManager();
    ~MultiCameraManager();

    //Có thể thêm một camera mới với URL RTSP
    void add_camera(const std::string& rtsp_url);
    
    //Bắt đầu tất cả các luồng camera
    void start();

    //Dừng tất cả các luồng camera
    void stop();

private:
    //Hàm chạy cho mỗi threadcamera
    void cameraThreadFunc(const std::string& rtsp_url, std::atomic<bool>& running_flag);

    std::vector<std::string> rtsp_url_;
    std::vector<std::thread> thread_;
    std::vector<std::unique_ptr<CameraContext>> cameras_;
    std::vector<std::unique_ptr<std::atomic<bool>>> running_flag_;
};

#endif