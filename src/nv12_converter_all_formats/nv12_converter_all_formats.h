#ifndef NV12_CONVERTER_ALL_FORMATS_H
#define NV12_CONVERTER_ALL_FORMATS_H

#include <iostream>
#include <memory>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <sys/ioctl.h>
#include <cstdint>
#include <cstddef>
#include <chrono>

// RGA Headers - sử dụng RGA v1 API cho hardware acceleration
extern "C" {
#include <rga/RgaApi.h>
}


    // Chuyển đổi NV12 DMABuf sang DMABuf (zero-copy hardware)
    bool convertDMABufToDMABuf(int src_dma_fd, int dst_dma_fd, int width, int height);

class NV12Converter {
public:
    NV12Converter();
    ~NV12Converter();
    
    // Khởi tạo RGA context
    bool init();
    
   // Chuyển đổi NV12 DMABuf sang định dạng bất kỳ (ví dụ: RGB, BGR, RGBA, ...)
    bool convertDMABufToAnyFormat(int dma_fd, int src_width, int src_height,
                                  int dst_width, int dst_height, int dst_format,
                                  uint8_t** out_data, size_t* out_size);
    
    // Chuyển đổi NV12 DMABuf sang định dạng DMABuf (zero-copy hardware acceleration)
     bool convertDMABufToRGB(int dma_fd, int src_width, int src_height,
                            int dst_width, int dst_height,
                            uint8_t** rgb_data, size_t* rgb_size);

    bool convertDMABufToDMABuf(int src_dma_fd, int dst_dma_fd, int width, int height);
    
    void cleanup();     // Cleanup

private:
    bool rga_initialized_;
    
    // Helper functions cho RGA hardware acceleration
    bool setupRgaInfo(rga_info_t* info, int fd, int width, int height, int format);
    void printRgaError(int ret);
        // Tạo RGB DMABuf buffer
    int allocateRGBDMABuf(int width, int height);
    int allocateDMABuf(int width, int height, int format, int bytes_per_pixel);
    // Giải phóng DMABuf
    void freeDMABuf(int dma_fd);
};

#endif // NV12_CONVERTER_H
