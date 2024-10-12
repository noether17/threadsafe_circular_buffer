#include <gtest/gtest.h>

#include <vector>

#include "ThreadSafeBuffer.hpp"

class ThreadSafeBufferTest : public testing::Test {
 protected:
  auto static constexpr buffer_size = 16;
  ThreadSafeBuffer<int, buffer_size> buffer{};
};

TEST_F(ThreadSafeBufferTest, AlternateWriteRead) {
  auto output_vector = std::vector<int>{};

  for (auto i = 0; i < 2 * buffer_size; ++i) {
    buffer.write_next(i);
    buffer.read_next([&output_vector](int i) { output_vector.push_back(i); });
  }

  EXPECT_EQ(2 * buffer_size, output_vector.size());
  for (auto i = 0; i < 2 * buffer_size; ++i) {
    EXPECT_EQ(i, output_vector[i]);
  }
}
