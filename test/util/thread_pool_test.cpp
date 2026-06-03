#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include "minisearch/util/ThreadPool.hpp"

namespace {

TEST(ThreadPoolTest, RunsSubmittedTasksAndReturnsValues) {
  minisearch::util::ThreadPool pool(2);

  auto number = pool.submit([]() -> int { return 42; });
  auto text = pool.submit([]() -> std::string { return "done"; });

  EXPECT_EQ(number.get(), 42);
  EXPECT_EQ(text.get(), "done");
}

TEST(ThreadPoolTest, ZeroThreadCountStillRunsTasks) {
  minisearch::util::ThreadPool pool(0);

  auto result = pool.submit([]() -> int { return 7; });

  EXPECT_EQ(result.get(), 7);
}

TEST(ThreadPoolTest, PropagatesTaskExceptionsThroughFuture) {
  minisearch::util::ThreadPool pool(1);

  auto result =
      pool.submit([]() -> int { throw std::runtime_error("task failed"); });

  EXPECT_THROW(result.get(), std::runtime_error);
}

}  // namespace
