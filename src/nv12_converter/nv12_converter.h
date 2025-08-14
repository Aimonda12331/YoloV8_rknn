#ifndef NV12_CONVERTER_H
#define NV12_CONVERTER_H

#include <iostream>
#include <memory>
#include <cstring>

// RGA Headers - sử dụng RGA v1 API cho hardware acceleration
extern "C" {
#include <rga/RgaApi.h>
}

class NV12Converter {
public:
    NV12Converter();
    ~NV12Converter();
    
    // Khởi tạo RGA context
    bool init();
    
    // Chuyển đổi NV12 DMABuf sang RGB24 (sử dụng RGA hardware)
    bool convertDMABufToRGB(int dma_fd, int width, int height, uint8_t** rgb_data, size_t* rgb_size);
    
    // Chuyển đổi NV12 DMABuf sang RGB DMABuf (zero-copy hardware acceleration)
    bool convertDMABufToDMABuf(int src_dma_fd, int dst_dma_fd, int width, int height);
    
    // Tạo RGB DMABuf buffer
    int allocateRGBDMABuf(int width, int height);
    
    // Giải phóng DMABuf
    void freeDMABuf(int dma_fd);
    
    // Cleanup
    void cleanup();

private:
    bool rga_initialized_;
    
    // Helper functions cho RGA hardware acceleration
    bool setupRgaInfo(rga_info_t* info, int fd, int width, int height, int format);
    void printRgaError(int ret);
};

#endif // NV12_CONVERTER_H
