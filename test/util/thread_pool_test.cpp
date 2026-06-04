#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "minisearch/util/ThreadPool.hpp"

namespace {

TEST(ThreadPoolTest, RunsSubmittedTasksAndReturnsValues) {
  minisearch::util::ThreadPool thread_pool(2);

  auto number_future = thread_pool.submit([]() -> int { return 42; });
  auto text_future = thread_pool.submit([]() -> std::string { return "done"; });

  EXPECT_EQ(number_future.get(), 42);
  EXPECT_EQ(text_future.get(), "done");
}

TEST(ThreadPoolTest, ZeroThreadCountStillRunsTasks) {
  minisearch::util::ThreadPool thread_pool(0);

  auto result_future = thread_pool.submit([]() -> int { return 7; });

  EXPECT_EQ(result_future.get(), 7);
}

TEST(ThreadPoolTest, PropagatesTaskExceptionsThroughFuture) {
  minisearch::util::ThreadPool thread_pool(1);

  auto result_future = thread_pool.submit(
      []() -> int { throw std::runtime_error("task failed"); });

  EXPECT_THROW(result_future.get(), std::runtime_error);
}

}  // namespace
