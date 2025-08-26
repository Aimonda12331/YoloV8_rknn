#include <multi_thread.h>
#include <CameraContext.h>

MultiCameraManager::MultiCameraManager() = default;
MultiCameraManager::~MultiCameraManager() = default;

// Hàm khởi tạo và chạy một camera RTSP trên một thread riêng.
// Tham số: url - đường dẫn RTSP của camera.
// Sử dụng: Gọi hàm này cho mỗi camera muốn thêm vào hệ thống.
void MultiCameraManager::add_camera(const std::string& url) {
    auto cam = std::make_unique<CameraContext>();
    cam->url = url;
    cam->reader = std::make_unique<rtsp_reader>(url);
    cameras_.push_back(std::move(cam));
    running_flag_.push_back(std::make_unique<std::atomic<bool>>(true));
}

void MultiCameraManager::start(){
    for(size_t i =0 ;i < cameras_.size(); ++i) { 
        cameras_[i]-> th = std::thread([this, i]() {
            cameraThreadFunc(cameras_[i]->url, *running_flag_[i]);
        });
    }
}

void MultiCameraManager::stop(){
    for (auto& flag : running_flag_) flag->store(false);
    for (auto& cam : cameras_){
        if(cam->th.joinable()) cam->th.join();
    }
}


// Hàm thực thi luồng xử lý cho một camera RTSP.
// Tham số:
//   - rtsp_url: Đường dẫn RTSP của camera.
//   - running_flag: Cờ điều khiển dừng/tiếp tục luồng.
// Sử dụng: Được gọi bởi thread riêng cho từng camera, xử lý nhận frame, chuyển đổi NV12 sang BGR qua RGA, và callback xử lý tiếp.
void MultiCameraManager::cameraThreadFunc(const std::string& rtsp_url, std::atomic<bool>& running_flag) {
    NV12Converter converter;
    if (!converter.init()) {
        std::cerr << "[ERROR] Không thể khởi tạo RGA converter cho camera: " << rtsp_url << std::endl;
        return;
    }

    rtsp_reader reader(rtsp_url);
    if (!reader.init()) {
        std::cerr << "[ERROR] Không thể khởi tạo pipeline RTSP cho camera: " << rtsp_url << std::endl;
        return;
    }

    reader.setFrameCallback([&](int dma_fd, int width, int height) {
        uint8_t* out_data = nullptr;
        size_t out_size = 0;
        int dst_format = RK_FORMAT_BGR_888; // Hoặc định dạng bạn muốn
        // int dst_format = RK_FORMAT_RGB_888;  // Cho hiển thị hoặc AI input RGB
        // int dst_format = RK_FORMAT_RGBA_8888;// Cho OpenGL texture
        // int dst_format = RK_FORMAT_YCbCr_420_SP; // NV12
        // int dst_format = RK_FORMAT_YCrCb_420_SP; // NV21
        // int dst_format = RK_FORMAT_RGB_565;  // Cho LCD framebuffer



        if (converter.convertDMABufToAnyFormat(
                dma_fd, width, height,
                width, height,
                dst_format,
                &out_data, &out_size)) {
            std::cout << "[CAM] " << rtsp_url << " | Converted to format " << dst_format
                      << " size: " << out_size << " bytes" << std::endl;
            // TODO: Đẩy out_data vào queue hoặc xử lý tiếp (AI, render, ghi file...)
            delete[] out_data;
        } else {
            std::cerr << "[CAM] " << rtsp_url << " | Lỗi chuyển đổi NV12!" << std::endl;
        }
    });

    reader.run();

    while (running_flag) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}