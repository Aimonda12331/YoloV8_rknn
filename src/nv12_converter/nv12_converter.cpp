#include "nv12_converter.h"
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <chrono>

//==================== CONSTRUCTOR & DESTRUCTOR ====================//
NV12Converter::NV12Converter() : rga_initialized_(false) {
    std::cout << "[INFO] NV12Converter created" << std::endl;
}

NV12Converter::~NV12Converter() {
    cleanup();
    std::cout << "[INFO] NV12Converter destroyed" << std::endl;
}

//==================== INIT RGA CONTEXT ====================//
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
bool NV12Converter::convertDMABufToRGB(int dma_fd, int width, int height, uint8_t** rgb_data, size_t* rgb_size) {
    if (!rga_initialized_) {
        std::cerr << "[ERROR] RGA hardware chưa được khởi tạo!" << std::endl;
        return false;
    }
    
    if (width <= 0 || height <= 0 || !rgb_data || !rgb_size) {
        std::cerr << "[ERROR] Tham số không hợp lệ!" << std::endl;
        return false;
    }
    
    // Tạo RGB DMABuf output buffer
    int dst_fd = allocateRGBDMABuf(width, height);
    if (dst_fd <= 0) {
        std::cerr << "[ERROR] Không thể allocate RGB DMABuf!" << std::endl;
        return false;
    }
    
    // Setup RGA info cho hardware acceleration
    rga_info_t src_info, dst_info;
    
    if (!setupRgaInfo(&src_info, dma_fd, width, height, RK_FORMAT_YCbCr_420_SP)) {
        std::cerr << "[ERROR] Không thể setup source RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }
    
    if (!setupRgaInfo(&dst_info, dst_fd, width, height, RK_FORMAT_RGB_888)) {
        std::cerr << "[ERROR] Không thể setup destination RGA info!" << std::endl;
        close(dst_fd);
        return false;
    }
    
    // Thực hiện hardware acceleration conversion
    std::cout << "[DEBUG] Converting NV12(" << width << "x" << height << ") DMABuf fd=" << dma_fd << " to RGB24 (RGA hardware)..." << std::endl;
    
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
    *rgb_size = width * height * 3;
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
void NV12Converter::freeDMABuf(int dma_fd) {
    if (dma_fd > 0) {
        close(dma_fd);
        std::cout << "[INFO] Freed DMABuf fd=" << dma_fd << std::endl;
    }
}

//==================== CLEANUP ====================//
void NV12Converter::cleanup() {
    if (rga_initialized_) {
        c_RkRgaDeInit();
        rga_initialized_ = false;
        std::cout << "[INFO] RGA hardware deinitialized" << std::endl;
    }
}

//==================== HELPER: SETUP RGA INFO ====================//
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
