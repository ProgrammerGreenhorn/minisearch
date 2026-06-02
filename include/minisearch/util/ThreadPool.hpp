#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace minisearch::util {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t threadCount);

  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;

  auto operator=(const ThreadPool &) -> ThreadPool & = delete;

  template <typename F>
  auto submit(F &&function) -> std::future<std::invoke_result_t<F>> {
    using Result = std::invoke_result_t<F>;

    auto task = std::make_shared<std::packaged_task<Result()>>(
        std::forward<F>(function));
    auto future = task->get_future();

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopping_) {
        throw std::runtime_error("cannot submit task to stopped thread pool");
      }

      tasks_.emplace([task]() -> void { (*task)(); });
    }

    condition_.notify_one();
    return future;
  }

 private:
  auto workerLoop() -> void;

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stopping_ = false;
};

}  // namespace minisearch::util
