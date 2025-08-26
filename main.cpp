#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <csignal>

#include <rtspProcess/mpp_rtspProcess.h>
#include <nv12_converter_all_formats/nv12_converter_all_formats.h>
#include <MultiCameraManager/multi_thread.h>

// Biến toàn cục để điều khiển dừng chương trình
std::atomic<bool> keepRunning(true);

// Hàm xử lý tín hiệu dừng (Ctrl+C)
void signalHandler(int signum) {
    keepRunning = false;
}

/**
 * @brief Chạy đa luồng nhiều camera RTSP với MultiCameraManager
 */
void run_multi_camera() {
    // Danh sách url rtsp của camera
    std::vector<std::string> rtsp_urls = {
        "rtsp://user03:abcd1234@113.177.126.84:8202",
        "rtsp://103.147.186.175:18554/9L02DA3PAJ39B2F",
        // "rtsp://103.147.186.175:18554/AA06E7CPAJ91031",
        // "rtsp://103.147.186.175:18554/AA06E7CPAJCD058"
        
        // Thêm các camera khác nếu muốn
    };

    // Khởi tạo MultiCameraManager và thêm các camera
    MultiCameraManager manager;
    for (const auto& url : rtsp_urls) {
        manager.add_camera(url);
    }

    // Bắt đầu chạy đa luồng các camera
    manager.start();

    std::cout << "[INFO] Đang chạy đa luồng camera..." << std::endl;
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // Khi nhận tín hiệu dừng, sẽ dừng tất cả các camera
    manager.stop();
    std::cout << "[INFO] Chương trình kết thúc." << std::endl;
}

int main() {
    // Đăng ký handler cho Ctrl+C
    std::signal(SIGINT, signalHandler);

    // Chạy hệ thống đa camera
    run_multi_camera();

    return 0;
}