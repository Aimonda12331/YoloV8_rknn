// Đoạn code bạn cung cấp là phần hiện thực một web server hiển thị video stream từ nhiều camera sử dụng thư viện Crow (C++ web framework) và OpenCV. Cụ thể:

// Chức năng chính:
// Khởi tạo web server (Crow) để phục vụ các luồng hình ảnh (stream) từ nhiều camera qua HTTP.
// Kết hợp và xử lý frame từ các camera, mã hóa thành JPEG, và cung cấp qua các endpoint như /stream, /stream/0, /stream/1.
// Trang web giao diện: Khi truy cập /, server trả về một trang HTML hiển thị hình ảnh từ các camera, tự động làm mới ảnh liên tục.
// Tính toán và hiển thị FPS (số khung hình/giây) cho từng camera và cho frame tổng hợp.
// Xử lý khi camera offline: Nếu không có frame, sẽ hiển thị thông báo "Camera X: Offline".
// Cách hoạt động:
// startWebServer(port): Khởi động web server trên cổng chỉ định, tạo các route để truy cập stream và trang web.
// updateFrames(frame1, frame2): Nhận 2 frame từ 2 camera, kết hợp lại, mã hóa JPEG, lưu vào bộ nhớ đệm để phục vụ qua web.
// updateFrame(frame, camera_id): Cập nhật frame cho từng camera riêng lẻ, mã hóa JPEG, lưu vào bộ nhớ đệm riêng cho từng camera.
// combineFrames: Kết hợp 2 frame thành một ảnh lớn, thêm timestamp và FPS.
// stopWebServer: Dừng web server.
// Ứng dụng thực tế:
// Xây dựng hệ thống giám sát camera IP, robot, AI camera, hoặc bất kỳ ứng dụng nào cần truyền hình ảnh/video real-time qua web.
// Có thể mở trình duyệt và xem trực tiếp hình ảnh từ nhiều camera trên cùng một giao diện.


// Để sử dụng phương pháp web server với Crow để hiển thị hình ảnh từ camera lên web, bạn có thể làm theo các bước sau. Dưới đây là hướng dẫn tổng quan, có thể áp dụng cho dự án của bạn:

// Bước 1: Thêm thư viện Crow vào dự án
// Tải Crow (https://github.com/CrowCpp/Crow) về, hoặc chỉ cần file crow_all.h (bản header-only).
// Đặt file crow_all.h vào thư mục dự án, ví dụ: third_party/crow_all.h.
// Thêm dòng #include "crow_all.h" vào file cần dùng.

// Bước 2: Đảm bảo dự án có OpenCV
// Dự án của bạn đã dùng OpenCV để xử lý hình ảnh.
// Đảm bảo CMakeLists.txt đã link OpenCV.
// Bước 3: Thiết kế class Stream/WebRTC
// Class này sẽ:
// Nhận frame từ các camera (có thể qua callback hoặc hàm update).
// Mã hóa frame thành JPEG.
// Lưu vào buffer.
// Tạo web server với Crow, cung cấp các route /, /stream, /stream/0, /stream/1...
// Bước 4: Kết nối luồng camera với Stream
// Khi nhận được frame từ camera (ví dụ qua rtsp_reader), gọi updateFrame hoặc updateFrames của class Stream để cập nhật hình ảnh mới.
// Bước 5: Chạy web server
// Gọi startWebServer(port) để khởi động server.
// Truy cập http://localhost:port/ để xem giao diện web.
// Bước 6: Tích hợp vào main
// Trong hàm main, khởi tạo Stream, kết nối với các camera, và chạy web server.
// Ví dụ tích hợp đơn giản:
// Lưu ý:
// Nếu muốn dùng nhiều camera, hãy đảm bảo mỗi camera có một thread lấy frame riêng.
// Nếu dùng RTSP, hãy dùng class rtsp_reader để lấy frame rồi truyền vào Stream.

