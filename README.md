<!-- <!-- ===GHI CHÚ===
Cài gstreamer libav (để có avdec_h264)
Chạy lệnh sau để cài plugin giải mã phần mềm:

sudo apt update
sudo apt install gstreamer1.0-libav

===TIẾN TRÌNH===
Bước 1. Khởi tạo camera (RTSPReader)
        Khởi tạo mô hình yolov8.rknn (Yolo8InitModel) 
Bước 2. Preprocess
    Mục tiêu: 
        Lấy hình từ RTSPReader
        Resize ảnh về kích thước đầu vào model (ví dụ 640x640)
        Chuyển màu BGR ➝ RGB
        (Tùy do_preprocess): Normalize và hoán vị HWC → CHW
 -->

ảnh lấy từ hệ thống có độ phân giải 1600x1200 dẫn đến CPU cao nên ép ảnh lại thành 640x480 --> áp dụng người

Tóm tắt logic
//reader
Constructor: khởi tạo GStreamer.
init(): tạo pipeline rtspsrc → rtph264depay → h264parse → queue → appsink và cấu hình các tham số.
run(): chạy vòng lặp nhận thông báo lỗi/EOS từ GStreamer
onNewSample(): callback khi có frame H.264 mới về từ appsink.
onPadAdded(): nối pad động từ rtspsrc vào depay.


#Lệnh unset DISPLAY để tắt hiển thị bằng X11