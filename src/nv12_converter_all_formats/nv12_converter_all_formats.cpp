#include "nv12_converter_all_formats.h"

//==================== CONSTRUCTOR & DESTRUCTOR ====================//
// Hàm khởi tạo NV12Converter.
// Khởi tạo trạng thái ban đầu, in log tạo đối tượng.
NV12Converter::NV12Converter() : rga_initialized_(false) {
    std::cout << "[INFO] NV12Converter created" << std::endl;
}

// Hàm hủy NV12Converter.
// Đảm bảo giải phóng tài nguyên RGA nếu còn, in log hủy đối tượng.
NV12Converter::~NV12Converter() {
    cleanup();
    std::cout << "[INFO] NV12Converter destroyed" << std::endl;
}

//==================== CHUYỂN ĐỔI DMABUF TO ANY FORMAT (RGA HARDWARE) ====================//
// Hàm chuyển đổi NV12 DMABuf sang định dạng bất kỳ (RGB, BGR, RGBA, ...).
// Sử dụng phần cứng RGA để thực hiện chuyển đổi và resize.
// Tham số:
//   - dma_fd: file descriptor của buffer NV12 đầu vào
//   - src_width, src_height: kích thước ảnh nguồn
//   - dst_width, dst_height: kích thước ảnh đích (resize)
//   - dst_format: mã định dạng đích (ví dụ: RK_FORMAT_BGR_888)
//   - out_data: con trỏ nhận buffer ảnh kết quả
//   - out_size: kích thước buffer kết quả
// Trả về true nếu thành công, false nếu lỗi.
bool NV12Converter::convertDMABufToAnyFormat(int dma_fd, int src_width, int src_height,
                                             int dst_width, int dst_height, int dst_format,
                                             uint8_t** out_data, size_t* out_size) {
    if (!rga_initialized_) {
        std::cerr << "[ERROR] RGA hardware chưa được khởi tạo!" << std::endl;
        return false;
    }
    if (src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0 || !out_data || !out_size) {
        std::cerr << "[ERROR] Tham số không hợp lệ!" << std::endl;
        return false;
    }

    // Tính số bytes mỗi pixel theo định dạng đích
    int bytes_per_pixel = 3;
    switch (dst_format) {
        case RK_FORMAT_RGB_888:
        case RK_FORMAT_BGR_888:
            bytes_per_pixel = 3; break;
        case RK_FORMAT_RGBA_8888:
        case RK_FORMAT_BGRA_8888:
            bytes_per_pixel = 4; break;
        case RK_FORMAT_YCbCr_420_SP:
        case RK_FORMAT_YCrCb_420_SP:
            bytes_per_pixel = 1; break;
        default:
            bytes_per_pixel = 3; break;
    }

    // Tạo DMABuf output buffer với định dạng đích
    int dst_fd = allocateDMABuf(dst_width, dst_height, dst_format, bytes_per_pixel);
    if (dst_fd <= 0) {
        std::cerr << "[ERROR] Không thể allocate DMABuf cho output!" << std::endl;
        return false;
    }

    rga_info_t src_info, dst_info;
    if (!setupRgaInfo(&src_info, dma_fd, src_width, src_height, RK_FORMAT_YCbCr_420_SP)) {
        std::cerr << "[ERROR] Không thể setup source RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }
    if (!setupRgaInfo(&dst_info, dst_fd, dst_width, dst_height, dst_format)) {
        std::cerr << "[ERROR] Không thể setup destination RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }

    std::cout << "[DEBUG] Converting NV12(" << src_width << "x" << src_height << ") DMABuf fd=" << dma_fd
              << " to format=" << dst_format << " (" << dst_width << "x" << dst_height << ")..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    int ret = c_RkRgaBlit(&src_info, &dst_info, NULL);
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    if (ret != 0) {
        std::cerr << "[ERROR] RGA hardware conversion failed: " << ret << std::endl;
        printRgaError(ret);
        close(dst_fd);
        return false;
    }

    *out_size = dst_width * dst_height * bytes_per_pixel;
    *out_data = new uint8_t[*out_size];
    if (!*out_data) {
        std::cerr << "[ERROR] Không thể allocate output buffer!" << std::endl;
        close(dst_fd);
        return false;
    }

    void* mapped_addr = mmap(NULL, *out_size, PROT_READ, MAP_SHARED, dst_fd, 0);
    if (mapped_addr == MAP_FAILED) {
        std::cerr << "[ERROR] Không thể mmap destination buffer!" << std::endl;
        delete[] *out_data;
        *out_data = nullptr;
        close(dst_fd);
        return false;
    }
    memcpy(*out_data, mapped_addr, *out_size);
    munmap(mapped_addr, *out_size);
    close(dst_fd);

    std::cout << "[INFO] RGA conversion thành công! Size: " << *out_size
              << " bytes, Time: " << duration << "ms" << std::endl;
    return true;
}

// Hàm bổ sung: cấp phát DMABuf cho định dạng bất kỳ
// Tạo buffer DMABuf với kích thước và định dạng mong muốn để lưu ảnh kết quả.
// Trả về file descriptor của buffer mới, hoặc -1 nếu lỗi.
int NV12Converter::allocateDMABuf(int width, int height, int format, int bytes_per_pixel) {
    size_t size = width * height * bytes_per_pixel;
    int heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (heap_fd < 0) return -1;

    struct dma_heap_allocation_data alloc = {};
    alloc.len = size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;
    alloc.heap_flags = 0;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
        close(heap_fd);
        return -1;
    }
    close(heap_fd);
    return alloc.fd;
}

//==================== INIT RGA CONTEXT ====================//
// Khởi tạo context phần cứng RGA.
// Gọi hàm khởi tạo driver RGA, trả về true nếu thành công.
bool NV12Converter::init() {
    // Khởi tạo RGA hardware
    int ret = c_RkRgaInit();
    if (ret != 0) {
        std::cerr << "[ERROR] RGA hardware init failed: " << ret << std::endl;
        // Fallback to software không được phép
        return false;
    }
    
    rga_initialized_ = true;
    std::cout << "[INFO] RGA hardware initialized successfully" << std::endl;
    return true;
}

//==================== CHUYỂN ĐỔI DMABUF TO RGB (RGA HARDWARE) ====================//
// Hàm chuyển đổi NV12 DMABuf sang RGB24 (RK_FORMAT_RGB_888) bằng phần cứng RGA.
// Có thể resize nếu truyền kích thước đích khác kích thước nguồn.
// Trả về true nếu thành công, false nếu lỗi.
bool NV12Converter::convertDMABufToRGB(int dma_fd, int src_width, int src_height, 
                                        int dst_width, int dst_height,
                                        uint8_t** rgb_data, size_t* rgb_size) {
    if (!rga_initialized_) {
        std::cerr << "[ERROR] RGA hardware chưa được khởi tạo!" << std::endl;
        return false;
    }
    
    if (src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0 || !rgb_data || !rgb_size) {
        std::cerr << "[ERROR] Tham số không hợp lệ!" << std::endl;
        return false;
    }
    
    // Tạo RGB DMABuf output buffer
    int dst_fd = allocateRGBDMABuf(dst_width, dst_height);
    if (dst_fd <= 0) {
        std::cerr << "[ERROR] Không thể allocate RGB DMABuf!" << std::endl;
        return false;
    }
    
    // Setup RGA info cho hardware acceleration
    rga_info_t src_info, dst_info;
    
    if (!setupRgaInfo(&src_info, dma_fd, src_width, src_height, RK_FORMAT_YCbCr_420_SP)) {
        std::cerr << "[ERROR] Không thể setup source RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }
    
    if (!setupRgaInfo(&dst_info, dst_fd, dst_width, dst_height, RK_FORMAT_RGB_888)) {
        std::cerr << "[ERROR] Không thể setup destination RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }
    
    // Thực hiện hardware acceleration conversion
    std::cout << "[DEBUG] Converting NV12(" << src_width << "x" << src_height << ") DMABuf fd=" << dma_fd << " to RGB24 (RGA hardware)..." << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int ret = c_RkRgaBlit(&src_info, &dst_info, NULL);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    if (ret != 0) {
        std::cerr << "[ERROR] RGA hardware conversion failed: " << ret << std::endl;
        printRgaError(ret);
        close(dst_fd);
        return false;
    }
    
    // Tính toán kích thước và copy dữ liệu từ hardware buffer
    *rgb_size = dst_width * dst_height * 3;
    *rgb_data = new uint8_t[*rgb_size];
    
    if (!*rgb_data) {
        std::cerr << "[ERROR] Không thể allocate RGB buffer!" << std::endl;
        close(dst_fd);
        return false;
    }
    
    // Map hardware buffer và copy sang CPU memory
    void* mapped_addr = mmap(NULL, *rgb_size, PROT_READ, MAP_SHARED, dst_fd, 0);
    
    if (mapped_addr == MAP_FAILED) {
        std::cerr << "[ERROR] Không thể mmap destination buffer!" << std::endl;
        delete[] *rgb_data;
        *rgb_data = nullptr;
        close(dst_fd);
        return false;
    }
    
    memcpy(*rgb_data, mapped_addr, *rgb_size);
    munmap(mapped_addr, *rgb_size);
    close(dst_fd);
    
    std::cout << "[INFO] RGA hardware conversion thành công! Size: " << *rgb_size 
              << " bytes, Time: " << duration << "ms" << std::endl;
    return true;
}

//==================== CHUYỂN ĐỔI DMABUF TO DMABUF (ZERO-COPY HARDWARE) ====================//
// Hàm chuyển đổi NV12 DMABuf sang RGB DMABuf (zero-copy, không copy về CPU).
// Dùng cho các trường hợp cần xử lý tiếp trên GPU hoặc phần cứng khác.
bool NV12Converter::convertDMABufToDMABuf(int src_dma_fd, int dst_dma_fd, int width, int height) {
    if (!rga_initialized_) {
        std::cerr << "[ERROR] RGA hardware chưa được khởi tạo!" << std::endl;
        return false;
}
    
    rga_info_t src_info, dst_info;
    
    // Setup source buffer (NV12 DMABuf)
    if (!setupRgaInfo(&src_info, src_dma_fd, width, height, RK_FORMAT_YCbCr_420_SP)) {
        std::cerr << "[ERROR] Không thể setup source RGA info!" << std::endl;
        return false;
}
    
    // Setup destination buffer (RGB DMABuf)
    if (!setupRgaInfo(&dst_info, dst_dma_fd, width, height, RK_FORMAT_RGB_888)) {
        std::cerr << "[ERROR] Không thể setup destination RGA info!" << std::endl;
        return false;
}
    
    // Thực hiện zero-copy hardware conversion
    std::cout << "[DEBUG] Converting NV12 DMABuf fd=" << src_dma_fd 
              << " to RGB DMABuf fd=" << dst_dma_fd << " (RGA zero-copy hardware)" << std::endl;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    int ret = c_RkRgaBlit(&src_info, &dst_info, NULL);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    if (ret != 0) {
        std::cerr << "[ERROR] RGA hardware zero-copy conversion failed: " << ret << std::endl;
        printRgaError(ret);
        return false;
    }
    
    std::cout << "[INFO] RGA zero-copy conversion thành công! Time: " << duration << "ms" << std::endl;
    return true;
}

//==================== ALLOCATE RGB DMABUF ====================//
// Tạo buffer DMABuf cho ảnh RGB24 (3 bytes/pixel).
// Trả về file descriptor của buffer mới, hoặc -1 nếu lỗi.
int NV12Converter::allocateRGBDMABuf(int width, int height) {
    size_t rgb_size = width * height * 3; // RGB24 = 3 bytes per pixel

    int heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (heap_fd < 0) {
        std::cerr << "[ERROR] Không mở được /dev/dma_heap/system: " << strerror(errno) << std::endl;
        return -1;
    }

    struct dma_heap_allocation_data alloc = {};
    alloc.len = rgb_size;
    alloc.fd_flags = O_CLOEXEC | O_RDWR;
    alloc.heap_flags = 0;

    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
        std::cerr << "[ERROR] ioctl DMA_HEAP_IOCTL_ALLOC thất bại: " << strerror(errno) << std::endl;
        close(heap_fd);
        return -1;
    }
    close(heap_fd);

    int fd = alloc.fd;
    if (fd < 0) {
        std::cerr << "[ERROR] Không lấy được fd từ DMA heap!" << std::endl;
        return -1;
    }
    std::cout << "[INFO] Allocated RGB DMABuf: fd=" << fd
              << " size=" << rgb_size << " bytes (" << width << "x" << height << ")" << std::endl;
    return fd;
}

//==================== FREE DMABUF ====================//
// Giải phóng buffer DMABuf bằng cách đóng file descriptor.
void NV12Converter::freeDMABuf(int dma_fd) {
    if (dma_fd > 0) {
        close(dma_fd);
        std::cout << "[INFO] Freed DMABuf fd=" << dma_fd << std::endl;
    }
}

//==================== CLEANUP ====================//
// Giải phóng context phần cứng RGA nếu đã khởi tạo.
void NV12Converter::cleanup() {
    if (rga_initialized_) {
        c_RkRgaDeInit();
        rga_initialized_ = false;
        std::cout << "[INFO] RGA hardware deinitialized" << std::endl;
    }
}

//==================== HELPER: SETUP RGA INFO ====================//
// Hàm thiết lập thông tin buffer cho RGA (fd, kích thước, định dạng, ...).
// Trả về true nếu thành công.
bool NV12Converter::setupRgaInfo(rga_info_t* info, int fd, int width, int height, int format) {
    if (!info) return false;
    
    memset(info, 0, sizeof(rga_info_t));
    info->fd = fd;
    info->mmuFlag = 1;  // Enable MMU cho DMABuf access
    
    // Set format
    info->format = format;
    
    // Set rect dimensions
    rga_set_rect(&info->rect, 0, 0, width, height, width, height, format);
    
    return true;
}

//==================== HELPER: PRINT RGA ERROR ====================//
// In ra thông báo lỗi chi tiết theo mã lỗi trả về từ RGA.
void NV12Converter::printRgaError(int ret) {
    switch (ret) {
        case 0:
            std::cout << "[RGA] Success" << std::endl;
            break;
        case -1:
            std::cerr << "[RGA ERROR] General error" << std::endl;
            break;
        case -2:
            std::cerr << "[RGA ERROR] Invalid parameter" << std::endl;
            break;
        case -3:
            std::cerr << "[RGA ERROR] Out of memory" << std::endl;
            break;
        case -4:
            std::cerr << "[RGA ERROR] Not supported" << std::endl;
            break;
        default:
            std::cerr << "[RGA ERROR] Unknown error: " << ret << std::endl;
            break;
    }
}
