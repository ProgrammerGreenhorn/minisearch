#include "minisearch/util/ThreadPool.hpp"

#include <algorithm>

namespace minisearch::util {

ThreadPool::ThreadPool(std::size_t threadCount) {
  threadCount = std::max<std::size_t>(1, threadCount);
  workers_.reserve(threadCount);

  for (std::size_t i = 0; i < threadCount; ++i) {
    workers_.emplace_back([this]() -> void { workerLoop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }

  condition_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) {
      worker.join();
    }
  }
}

auto ThreadPool::workerLoop() -> void {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      condition_.wait(
          lock, [this]() -> bool { return stopping_ || !tasks_.empty(); });

      if (stopping_ && tasks_.empty()) {
        return;
      }

      task = std::move(tasks_.front());
      tasks_.pop();
    }

    task();
  }
}

}  // namespace minisearch::util
