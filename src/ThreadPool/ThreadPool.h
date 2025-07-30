// ThreadPool là một công cụ đa luồng mạnh mẽ giúp xử lý nhiều công việc
// đồng thời mà không cần phải tự quản lý từng std::thread —
// rất phù hợp cho các ứng dụng realtime, xử lý ảnh, AI, server 
// hoặc bất kỳ thứ gì cần chạy nhanh và mượt.

#ifndef THREAD_POOL_H     // Tránh include trùng lặp file (include guard)
#define THREAD_POOL_H

// Các thư viện cần thiết cho đa luồng và hàng đợi tác vụ
#include <functional>           // std::function — đại diện cho "hàm" hoặc lambda
#include <thread>               // std::thread — để tạo và quản lý luồng
#include <mutex>                // std::mutex — để khóa bảo vệ vùng dữ liệu dùng chung
#include <condition_variable>   // std::condition_variable — để đồng bộ giữa các luồng (chờ và đánh thức)
#include <queue>                // std::queue — hàng đợi chứa các tác vụ
#include <vector>               // std::vector — danh sách các luồng
#include <atomic>               // std::atomic<bool> — biến bool dùng giữa nhiều luồng mà không cần mutex

class ThreadPool{// Lớp ThreadPool — quản lý nhiều worker thread để xử lý các tác vụ không đồng bộ
public:

    ThreadPool(size_t numThreads);              // Hàm khởi tạo — truyền vào số lượng luồng muốn sử dụng
    ~ThreadPool();                              // Hàm hủy — đảm bảo giải phóng tài nguyên, dừng các luồng đúng cách
    void enqueue(std::function<void()> task);   // Hàm thêm một tác vụ mới vào hàng đợi

private:

    std::vector<std::thread> workers;           // Danh sách các luồng worker đang chạy trong pool
    std::queue<std::function<void()>> tasks;    // Hàng đợi các tác vụ (lambda, hàm, v.v.)
    std::mutex queueMutex;                      // Mutex dùng để khóa hàng đợi khi nhiều luồng truy cập
    std::condition_variable condition;          // Biến điều kiện để worker chờ khi hàng đợi rỗng và đánh thức khi có task mới
    std::atomic<bool> stop;                     // Biến flag dùng để báo hiệu dừng tất cả các worker (an toàn giữa nhiều luồng)

};

#endif // THREAD_POOL_H
