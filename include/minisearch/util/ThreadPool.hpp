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
  /**
   * @brief Start a thread pool with the requested number of workers.
   *
   * @param thread_count Number of worker threads to create.
   */
  explicit ThreadPool(std::size_t thread_count);

  /**
   * @brief Stop workers and wait for them to exit.
   */
  ~ThreadPool();

  /**
   * @brief Disable copy construction.
   *
   * @param other Thread pool that would have been copied.
   */
  ThreadPool(const ThreadPool &other) = delete;

  /**
   * @brief Disable copy assignment.
   *
   * @param other Thread pool that would have been assigned.
   * @return Reference to this thread pool.
   */
  auto operator=(const ThreadPool &other) -> ThreadPool & = delete;

  /**
   * @brief Submit a task to run on the worker pool.
   *
   * @tparam F Callable type.
   * @param task_function Callable task with no arguments.
   * @return Future that resolves to the callable result.
   */
  template <typename F>
  auto submit(F &&task_function) -> std::future<std::invoke_result_t<F>> {
    using Result = std::invoke_result_t<F>;

    std::shared_ptr<std::packaged_task<Result()>> packaged_task =
        std::make_shared<std::packaged_task<Result()>>(
            std::forward<F>(task_function));
    std::future<Result> task_future = packaged_task->get_future();

    {
      std::lock_guard<std::mutex> queue_lock(mutex_);
      if (stopping_) {
        throw std::runtime_error("cannot submit task to stopped thread pool");
      }

      tasks_.emplace([packaged_task]() -> void { (*packaged_task)(); });
    }

    condition_.notify_one();
    return task_future;
  }

 private:
  /**
   * @brief Run the worker loop for one background thread.
   */
  auto workerLoop() -> void;

  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool stopping_ = false;
};

}  // namespace minisearch::util
