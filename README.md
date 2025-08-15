# YOLOv8 RKNN - Video Processing Pipeline

Hệ thống xử lý video realtime với YOLOv8 trên Rockchip NPU, tối ưu hiệu năng với hardware acceleration.

## Tổng quan tiến trình

### **Giai đoạn 1: Video Input Pipeline** **(Hiện tại)**
- **RTSP Stream Reception**: Nhận video từ camera IP qua RTSP protocol
- **Hardware Decode**: Giải mã H.264 bằng Rockchip MPP (Media Process Platform)
- **Format Conversion**: Chuyển đổi NV12 → RGB24 bằng RGA (Rockchip Graphics Acceleration)
- **Zero-copy DMABuf**: Tối ưu hiệu năng với DMA buffer, giảm thiểu copy dữ liệu

### **Giai đoạn 2: AI Inference Pipeline** **(Chuẩn bị)*
- **Preprocess**: Resize, normalize, format conversion cho YOLOv8 input
- **YOLOv8 Inference**: Chạy object detection trên Rockchip NPU
- **Postprocess**: NMS, confidence filtering, bounding box calculation

### **Giai đoạn 3: Output Pipeline** **(Tương lai)**
- **Visualization**: Vẽ bounding box, labels với OpenGL
- **Stream Output**: Truyền kết quả về server qua GStreamer
- **Multi-threading**: Tối ưu pipeline với đa luồng

---

## Tiến trình hiện tại (Giai đoạn 1)

### **Pipeline Architecture**
```
RTSP Camera → GStreamer → MPP Decoder → RGA Converter → RGB Outpu
```

### **Các module đang hoạt động:**

#### 1. **RTSP Reader (`src/rtspProcess/mpp_rtspProcess.cpp`)**
- **Pipeline GStreamer**: `rtspsrc → rtph264depay → h264parse → queue → mppvideodec → appsink`
- **Hardware Decode**: Sử dụng `mppvideodec` để giải mã H.264 trên VPU
- **DMABuf Output**: Xuất frame NV12 dạng DMABuf (zero-copy)
- **Realtime Performance**: Latency thấp, FPS ổn định

#### 2. **NV12 Converter (`src/nv12_converter/nv12_converter.cpp`)**
- **RGA Hardware**: Chuyển đổi NV12 → RGB24 bằng phần cứng RGA
- **DMABuf Support**: Xử lý trực tiếp DMABuf, không copy qua CPU
- **High Performance**: ~4-5ms/frame conversion time (1600x1200)
- **Memory Management**: Tự động allocate/free DMA buffer

#### 3. **Main Controller (`main.cpp`)**
- **Pipeline Orchestration**: Điều phối toàn bộ quá trình
- **Callback System**: Xử lý frame thông qua callback asynchronous
- **Resource Management**: Quản lý lifecycle của các module

### **Hiệu năng hiện tại:**
- **Resolution**: 1600x1200 (1.92MP)
- **Frame Rate**: Realtime (25-30 FPS)
- **Conversion Time**: 4-5ms/frame (RGA hardware)
- **CPU Usage**: ~5-10% (chủ yếu coordination)
- **VPU Usage**: ~90% (decode)
- **RGA Usage**: ~90% (format conversion)

### **Tài nguyên hệ thống:**
- **CPU**: Chủ yếu điều phối, copy nhẹ từ DMABuf
- **VPU**: Giải mã video H.264 hoàn toàn
- **RGA**: Chuyển đổi format NV12→RGB hoàn toàn
- **GPU**: Không sử dụng (dành cho giai đoạn 3)
- **Memory**: DMABuf zero-copy, tối ưu băng thông

---

## Build & Run

### **Dependencies**
```bash
# GStreamer và plugins
sudo apt update
sudo apt install gstreamer1.0-libav gstreamer1.0-plugins-*

# Rockchip libraries
sudo apt install librga-dev libmpp-dev

# OpenCV (cho tương lai)
sudo apt install libopencv-dev
```

### **Build**
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### **Run**
```bash
# Tắt X11 display (nếu chạy headless)
unset DISPLAY

# Chạy chương trình
./build/yolo8
```

---

## Cấu trúc project

```
YoloV8_rknn/
├── main.cpp                    # Entry point và pipeline controller
├── src/
│   ├── rtspProcess/
│   │   ├── mpp_rtspProcess.cpp  # RTSP + MPP decode pipeline
│   │   ├── mpp_rtspProcess.h
│   │   └── openGL_rtspProcess.* # OpenGL rendering (inactive)
│   ├── nv12_converter/
│   │   ├── nv12_converter.cpp   # RGA hardware conversion
│   │   └── nv12_converter.h
│   ├── ThreadPool/              # Multi-threading (future)
│   └── Yolo8InitModel/          # AI inference (future)
├── rknn_model/
│   ├── yolov8.rknn             # YOLOv8 model for RKNN
│   ├── labels.txt              # Object class labels
│   └── rknpu2_yolo8.cpp        # RKNN inference code
└── CMakeLists.txt              # Build configuration
```

**Chú thích:**
- **Active**: Module đang hoạt động
- **Ready**: Module đã code nhưng chưa integrate
- **Future**: Module dự kiến phát triển

---

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

---

## 🛠 Upcoming Features

### **Giai đoạn 2: AI Integration**
- [ ] Integrate YOLOv8 inference với RKNN runtime
- [ ] Preprocess pipeline cho AI input
- [ ] Postprocess và NMS filtering
- [ ] Performance benchmarking

### **Giai đoạn 3: Advanced Features**
- [ ] OpenGL rendering với bounding box overlay
- [ ] Multi-threading cho parallel processing
- [ ] GStreamer output pipeline (stream to server)
- [ ] Configuration file support
- [ ] Real-time performance monitoring

---

## Performance Notes

- **Optimal Resolution**: 1600x1200 hiện tại ổn định
- **Hardware Acceleration**: Tận dụng tối đa VPU + RGA
- **Memory Efficiency**: Zero-copy DMABuf workflow
- **Scalability**: Dễ dàng mở rộng cho AI và visualization

---

## Development Guidelines

1. **Module Isolation**: Mỗi module độc lập, interface rõ ràng
2. **Hardware First**: Ưu tiên sử dụng hardware acceleration
3. **Zero-copy**: Tối ưu memory bandwidth với DMABuf
4. **Error Handling**: Comprehensive error checking và logging
5. **Documentation**: Comment code và update README theo tiến độ