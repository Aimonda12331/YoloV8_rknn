#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include <rtspProcess/mpp_rtspProcess.h>
// #include <rtspProcess/openGL_rtspProcess.h>
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
            
            // Backup: Option 1b - Full resolution (uncomment nếu cần full resolution)
            /*
            if (converter->convertDMABufToRGB(dma_fd, width, height, &rgb_data, &rgb_size)) {
                std::cout << "[SUCCESS] Converted to RGB! Size: " << rgb_size << " bytes" << std::endl;
                delete[] rgb_data;
            }
            */
            
            // Option 2: Zero-copy conversion (uncomment nếu cần)
            /*
            int rgb_dma_fd = converter->allocateRGBDMABuf(width, height);
            if (rgb_dma_fd > 0) {
                if (converter->convertDMABufToDMABuf(dma_fd, rgb_dma_fd, width, height)) {
                    std::cout << "[SUCCESS] Zero-copy NV12→RGB DMABuf conversion!" << std::endl;
                    // TODO: Sử dụng rgb_dma_fd cho processing tiếp
                }
                converter->freeDMABuf(rgb_dma_fd);
            }
            */
        }
        
        // Đẩy frame vào OpenGL viewer để hiển thị (không copy CPU)
        // glViewer.pushFrame(dma_fd, width, height);
    });
    
    return true;
}

//==================== INIT RTSP UPLOAD ====================//
// ⚠️⚠️⚠️ TODO: ĐANG TẠM ẨN UPLOAD RTSP - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
/*
bool initRTSP_Upload(const std::string& upload_url, int width, int height, int framerate) {
    if (!reader) {
        std::cerr << "[ERROR] RTSP reader chưa được khởi tạo!" << std::endl;
        return false;
    }

    if (!reader->init_pipeline_upload(upload_url, width, height, framerate)) {
        std::cerr << "[ERROR] Không thể khởi tạo pipeline upload RTSP!" << std::endl;
        return false;
    }

    reader->setUploadCallback([&](int dma_fd, int width, int height) {
        // Đẩy frame vào pipeline upload
        reader->push_frame_to_upload(dma_fd, width, height);
    });

    std::cout << "[INFO] Đã khởi tạo pipeline upload RTSP." << std::endl;
    return true;
}
*/

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

    
    // ⚠️⚠️⚠️ TODO: TẠM ẨN UPLOAD INIT - ENABLE LẠI KHI CẦN ⚠️⚠️⚠️
    // Ví dụ khởi tạo pipeline upload RTSP (URL gốc để test)
    // std::string upload_url= "rtsp://103.147.186.216:20990/livestream/00_11_22_33_44/index.rtsp?username=1124a9a7183d0257842986fe3e83fc21&time=UTC&key=&token=";
    // initRTSP_Upload(upload_url, 1600, 1200, 25);  // Giữ nguyên resolution input

    mainLoop();

    std::cout << "[INFO] Chương trình kết thúc." << std::endl;
    delete reader;
    delete converter;
    return 0;
}