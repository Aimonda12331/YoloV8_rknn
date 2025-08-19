

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>

#include <rtspProcess/mpp_rtspProcess.h>
#include <nv12_converter_all_formats/nv12_converter_all_formats.h>

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
            uint8_t* out_data = nullptr;
            size_t out_size = 0;

            // ===== DANH SÁCH ĐỊNH DẠNG CÓ THỂ SỬ DỤNG VỚI convertDMABufToAnyFormat =====
            // RK_FORMAT_RGB_888   : Ảnh RGB 24-bit (3 bytes/pixel)
            // RK_FORMAT_BGR_888   : Ảnh BGR 24-bit (3 bytes/pixel)
            // RK_FORMAT_RGBA_8888 : Ảnh RGBA 32-bit (4 bytes/pixel)
            // RK_FORMAT_BGRA_8888 : Ảnh BGRA 32-bit (4 bytes/pixel)
            // RK_FORMAT_YCbCr_420_SP : NV12 (1 byte/pixel)
            // RK_FORMAT_YCrCb_420_SP : NV21 (1 byte/pixel)
            // ... (các định dạng khác nếu driver RGA hỗ trợ)
            int dst_format = RK_FORMAT_BGR_888; // Đổi sang định dạng bạn muốn

            std::cout << "[DEBUG] Gọi convertDMABufToRGB với width=" << width << ", height=" << height << std::endl;
            
            if (converter->convertDMABufToAnyFormat(
                dma_fd, width, height,
                width, height,
                dst_format,
                &out_data, &out_size)) {
                std::cout << "[SUCCESS] Converted to RGB! Size: " << out_size << " bytes" << std::endl;
                
                // TODO: Sử dụng rgb_data cho AI inference hoặc xử lý khác
                
                std::cout << "[INFO] Resolution: " << width << "x" << height << std::endl;
                   
                // Xử lý out_data
                delete[] out_data;
            } else {
                std::cerr << "[ERROR] Không chuyển đổi được NV12 sang RGB!" << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    });

    return true;
}

//==================== ENTRY POINT ====================//
int main() {
    std::string rtsp_url = "rtsp://user03:abcd1234@113.177.126.84:8202";
    if (!initRTSP_Process(rtsp_url)) return -1;

    if (reader) {
        reader->run(); // Bắt đầu chạy pipeline GStreamer để nhận frame
    }

    // Giả sử bạn có một vòng lặp chính để giữ chương trình chạy
    std::cout << "[INFO] Đang chạy, nhấn Ctrl+C để thoát..." << std::endl;
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "[INFO] Chương trình kết thúc." << std::endl;
    delete reader;
    delete converter;
    return 0;
}