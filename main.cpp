#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include <rtspProcess/mpp_rtspProcess.h>
#include <nv12_converter/nv12_converter.h>

// Biến toàn cục
std::atomic<bool> keepRunning(true);
rtsp_reader* reader = nullptr;
// OpenGL_DMABuf glViewer;   // Ẩn tạm thời OpenGL viewer
NV12Converter* converter = nullptr;

//==================== INIT RTSP READER ====================//
bool initRTSP_Process(const std::string& rtsp_url) {
    // Khởi tạo RGA converter
    converter = new NV12Converter();
    if (!converter->init()) {
        std::cerr << "[ERROR] Không thể khởi tạo RGA converter!" << std::endl;
        delete converter;
        converter = nullptr;
        return false;
    }
    
    reader = new rtsp_reader(rtsp_url);
    if (!reader->init()) {
        std::cerr << "[ERROR] Không thể khởi tạo pipeline RTSP!" << std::endl;
        return false;
    }
    
    // Callback nhận DMABUF và chuyển đổi sang RGB
    reader->setFrameCallback([&](int dma_fd, int width, int height) {
        // ✅ Đã nhận được fd NV12 DMABUF từ mppvideodec
        std::cout << "[CALLBACK] Nhận NV12 DMABuf fd=" << dma_fd 
                  << " size=" << width << "x" << height << std::endl;
        
        if (converter) {
            // Option 1: Chuyển đổi sang RGB với downscaling (nhanh hơn)
            uint8_t* rgb_data = nullptr;
            size_t rgb_size = 0;
           
             std::cout << "[DEBUG] Gọi convertDMABufToRGB với width=" << width << ", height=" << height << std::endl;
            if (converter->convertDMABufToRGB(dma_fd, width, height, &rgb_data, &rgb_size)) {
                  std::cout << "[SUCCESS] Converted to RGB! Size: " << rgb_size << " bytes" << std::endl;
            
            // TODO: Sử dụng rgb_data cho AI inference hoặc xử lý khác
            // Kích thước: width x height
            std::cout << "[INFO] Resolution: " << width << "x" << height << std::endl;
            
                // Giải phóng memory
                delete[] rgb_data;
            } else {
                std::cerr << "[ERROR] Chuyển đổi NV12→RGB downscaled thất bại!" << std::endl;
            }
        }
    }
);
    return true;
}

//==================== MAIN LOOP ====================//
void mainLoop() {
    // Chạy RTSP reader trên thread riêng
    std::thread rtspThread([&]() {
        reader->run();   // blocking loop
        keepRunning = false;
    });

    // Ẩn toàn bộ phần hiển thị OpenGL
    std::cout << "[INFO] Nhấn q để thoát.\n";

    while (keepRunning) {
        if (std::cin.rdbuf()->in_avail()) {
            char c = std::cin.get();
            if (c == 'q') {
                keepRunning = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    rtspThread.join();
}

//==================== ENTRY POINT ====================//
int main() {
    std::string rtsp_url = "rtsp://user03:abcd1234@113.177.126.32:8153";
    if (!initRTSP_Process(rtsp_url)) return -1;

    mainLoop();

    std::cout << "[INFO] Chương trình kết thúc." << std::endl;
    delete reader;
    delete converter;
    return 0;
}