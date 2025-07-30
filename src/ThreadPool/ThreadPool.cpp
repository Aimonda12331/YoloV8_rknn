#include "ThreadPool/ThreadPool.h"

// Hàm khởi tạo ThreadPool với số lượng luồng worker được chỉ định
ThreadPool::ThreadPool(size_t numThreads) : stop(false) // Ban đầu không dừng
{
    for (size_t i = 0; i < numThreads; ++i) // Lặp để tạo numThreads luồng
    {
        // Thêm mỗi luồng vào vector workers
        workers.emplace_back([this]() {
            // Mỗi luồng chạy vòng lặp để lấy và xử lý task từ hàng đợi
            while (!stop) {
                std::function<void()> task; // Biến để chứa task lấy từ hàng đợi

                {
                    // Tạo vùng khóa để bảo vệ truy cập tasks
                    std::unique_lock<std::mutex> lock(queueMutex);

                    // Chờ đến khi có task mới hoặc tín hiệu dừng
                    condition.wait(lock, [this]() {
                        return stop || !tasks.empty(); // Chỉ tiếp tục khi có task hoặc cần dừng
                    });

                    // Nếu đã stop và hàng đợi trống → thoát vòng lặp
                    if (stop && tasks.empty()) return;

                    // Lấy task đầu tiên ra khỏi hàng đợi
                    task = std::move(tasks.front());
                    tasks.pop(); // Xóa task khỏi hàng đợi
                }

                task(); // Thực thi task
            }
        });
    }
}

// Hàm hủy ThreadPool: dừng tất cả các luồng và giải phóng tài nguyên
ThreadPool::~ThreadPool()
{
    stop = true;              // Báo hiệu tất cả worker thread dừng lại
    condition.notify_all();   // Đánh thức tất cả worker đang chờ task

    for (auto &worker : workers)
        worker.join();        // Chờ từng luồng hoàn thành trước khi hủy
}

// Hàm thêm một task mới vào hàng đợi
void ThreadPool::enqueue(std::function<void()> task)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex); // Bảo vệ khi thêm task
        tasks.push(std::move(task)); // Đưa task mới vào hàng đợi
    }

    condition.notify_one(); // Báo cho 1 worker biết rằng có task mới
}
