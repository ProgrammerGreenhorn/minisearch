#include "minisearch/util/ThreadPool.hpp"

#include <algorithm>

namespace minisearch::util {

ThreadPool::ThreadPool(std::size_t thread_count) {
  thread_count = std::max<std::size_t>(1, thread_count);
  workers_.reserve(thread_count);

  for (std::size_t worker_index = 0; worker_index < thread_count;
       ++worker_index) {
    workers_.emplace_back([this]() -> void { workerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> queue_lock(mutex_);
    stopping_ = true;
  }

  condition_.notify_all();
  for (auto& worker_thread : workers_) {
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  }
}

auto ThreadPool::workerLoop() -> void {
  while (true) {
    std::function<void()> pending_task;
    {
      std::unique_lock<std::mutex> queue_lock(mutex_);
      condition_.wait(queue_lock, [this]() -> bool {
        return stopping_ || !tasks_.empty();
      });

      if (stopping_ && tasks_.empty()) {
        return;
      }

      pending_task = std::move(tasks_.front());
      tasks_.pop();
    }

    pending_task();
  }
}

}  // namespace minisearch::util
