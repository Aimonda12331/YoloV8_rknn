<!-- <!-- ===GHI CHÚ===
Cài gstreamer libav (để có avdec_h264)
Chạy lệnh sau để cài plugin giải mã phần mềm:

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