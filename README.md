 
# YOLOv8 RKNN - Video Processing Pipeline

Hệ thống xử lý video realtime với YOLOv8 trên Rockchip NPU, tối ưu hiệu năng với hardware acceleration.

## Tổng quan pipeline hiện tại

- Nhận video từ camera IP qua RTSP (GStreamer)
- Giải mã H.264 bằng Rockchip MPP (Media Process Platform)
- Chuyển đổi NV12 sang nhiều định dạng (RGB24, BGR24, RGBA, BGRA, NV12, NV21, ...) bằng RGA (Rockchip Graphics Acceleration)
- Tối ưu hiệu năng với zero-copy DMABuf
- Chuẩn bị tích hợp AI inference (YOLOv8 trên RKNN)

## Build & Run

### Dependencies
```bash
sudo apt update
sudo apt install g++ cmake pkg-config
sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-* gstreamer1.0-tools
sudo apt install librga-dev libmpp-dev libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev
sudo apt install libopencv-dev
```

### Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run
```bash
unset DISPLAY # Nếu chạy headless
./build/yolo8
```

## Cấu trúc project

```
YoloV8_rknn/
├── main.cpp                    # Entry point và pipeline controller
├── src/
│   ├── rtspProcess/
│   │   ├── mpp_rtspProcess.cpp  # RTSP + MPP decode pipeline
│   │   ├── mpp_rtspProcess.h
│   ├── nv12_converter_all_formats/
│   │   ├── nv12_converter_all_formats.cpp   # RGA hardware conversion, đa định dạng
│   │   └── nv12_converter_all_formats.h
│   ├── ThreadPool/              # Multi-threading (future)
│   └── Yolo8InitModel/          # AI inference (future)
├── rknn_model/
│   ├── yolov8.rknn             # YOLOv8 model for RKNN
│   ├── labels.txt              # Object class labels
│   └── rknpu2_yolo8.cpp        # RKNN inference code
└── CMakeLists.txt              # Build configuration
```

## Hướng dẫn đổi định dạng đầu ra (NV12 → RGB/BGR/RGBA/...)

Bạn có thể chọn định dạng đầu ra mong muốn khi gọi hàm chuyển đổi trong code:

```cpp
int dst_format = RK_FORMAT_BGR_888; // hoặc RK_FORMAT_RGB_888, RK_FORMAT_RGBA_8888, ...
converter->convertDMABufToAnyFormat(
	dma_fd, width, height,
	width, height,
	dst_format,
	&out_data, &out_size
);
```

**Các định dạng phổ biến:**
- `RK_FORMAT_RGB_888`   : Ảnh RGB 24-bit (3 bytes/pixel)
- `RK_FORMAT_BGR_888`   : Ảnh BGR 24-bit (3 bytes/pixel)
- `RK_FORMAT_RGBA_8888` : Ảnh RGBA 32-bit (4 bytes/pixel)
- `RK_FORMAT_BGRA_8888` : Ảnh BGRA 32-bit (4 bytes/pixel)
- `RK_FORMAT_YCbCr_420_SP` : NV12 (1 byte/pixel)
- `RK_FORMAT_YCrCb_420_SP` : NV21 (1 byte/pixel)

> **Lưu ý:** Định dạng hỗ trợ phụ thuộc vào driver RGA của thiết bị.

## Log mẫu (Runtime)

```
[INFO] Pipeline đang chạy...
[INFO] Pad RTSP nối thành công!
[INFO] Nhận NV12 DMABUF fd=40 size=1600x1200
[CALLBACK] Nhận NV12 DMABuf fd=40 size=1600x1200
[DEBUG] Converting NV12(1600x1200) DMABuf fd=40 to RGB24 (RGA hardware)...
[INFO] RGA hardware conversion thành công! Size: 5760000 bytes, Time: 4ms
[SUCCESS] Converted to RGB! Size: 5760000 bytes
[INFO] FPS = 25
```

## Roadmap & Upcoming Features

- [ ] Tích hợp YOLOv8 inference với RKNN runtime
- [ ] Preprocess pipeline cho AI input
- [ ] Postprocess và NMS filtering
- [ ] OpenGL rendering với bounding box overlay
- [ ] Multi-threading cho parallel processing
- [ ] GStreamer output pipeline (stream to server)
- [ ] Real-time performance monitoring

## Performance Notes

- **Optimal Resolution**: 1600x1200 hiện tại ổn định
- **Hardware Acceleration**: Tận dụng tối đa VPU + RGA
- **Memory Efficiency**: Zero-copy DMABuf workflow
- **Scalability**: Dễ dàng mở rộng cho AI và visualization

## Development Guidelines

1. **Module Isolation**: Mỗi module độc lập, interface rõ ràng
2. **Hardware First**: Ưu tiên sử dụng hardware acceleration
3. **Zero-copy**: Tối ưu memory bandwidth với DMABuf
4. **Error Handling**: Comprehensive error checking và logging
5. **Documentation**: Comment code và update README theo tiến độ