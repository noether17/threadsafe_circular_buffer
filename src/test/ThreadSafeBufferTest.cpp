#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

#include "ThreadSafeBuffer.hpp"

class ThreadSafeBufferTest : public testing::Test {
 protected:
  auto static constexpr buffer_size = 16;
  auto static constexpr n_passes = 2;
  auto static constexpr n_values = n_passes * buffer_size;

  ThreadSafeBuffer<int, buffer_size> buffer{};
};

TEST_F(ThreadSafeBufferTest, SingleThreadAlternateWriteRead) {
  auto output_vector = std::vector<int>{};

  for (auto i = 0; i < n_values; ++i) {
    buffer.write_next(i);
    buffer.read_next([&output_vector](int i) { output_vector.push_back(i); });
  }

  EXPECT_EQ(n_values, output_vector.size());
  for (auto i = 0; i < n_values; ++i) {
    EXPECT_EQ(i, output_vector[i]);
  }
}

TEST_F(ThreadSafeBufferTest, MultipleWritersSingleReader) {
  auto writers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};

  for (auto i = 0; i < n_values; ++i) {
    writers.push_back(std::jthread([this](int i) { buffer.write_next(i); }, i));
  }
  for (auto i = 0; i < n_values; ++i) {
    buffer.read_next([&output_vector](int i) { output_vector.push_back(i); });
  }

  EXPECT_EQ(n_values, output_vector.size());
  std::sort(output_vector.begin(), output_vector.end());
  for (auto i = 0; auto const& x : output_vector) {
    EXPECT_EQ(i++, x);
  }
}

TEST_F(ThreadSafeBufferTest, SingleWriterMultipleReaders) {
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  // need to start readers before writing on main thread
  for (auto i = 0; i < n_values; ++i) {
    readers.push_back(
        std::jthread([this](auto read_func) { buffer.read_next(read_func); },
                     [&output_vector, &output_mx](int i) {
                       auto lock = std::lock_guard{output_mx};
                       output_vector.push_back(i);
                     }));
  }
  for (auto i = 0; i < n_values; ++i) {
    buffer.write_next(i);
  }
  for (auto& r : readers) {
    r.join();
  }

  EXPECT_EQ(n_values, output_vector.size());
  std::sort(output_vector.begin(), output_vector.end());
  for (auto i = 0; auto const& x : output_vector) {
    EXPECT_EQ(i++, x);
  }
}

TEST_F(ThreadSafeBufferTest, MultipleWritersMultipleReadersWriteFirst) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_values; ++i) {
    writers.push_back(std::jthread([this](int i) { buffer.write_next(i); }, i));
  }
  for (auto i = 0; i < n_values; ++i) {
    readers.push_back(
        std::jthread([this](auto read_func) { buffer.read_next(read_func); },
                     [&output_vector, &output_mx](int i) {
                       auto lock = std::lock_guard{output_mx};
                       output_vector.push_back(i);
                     }));
  }
  for (auto& r : readers) {
    r.join();
  }

  EXPECT_EQ(n_values, output_vector.size());
  std::sort(output_vector.begin(), output_vector.end());
  for (auto i = 0; auto const& x : output_vector) {
    EXPECT_EQ(i++, x);
  }
}

TEST_F(ThreadSafeBufferTest, MultipleWritersMultipleReadersReadFirst) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_values; ++i) {
    readers.push_back(
        std::jthread([this](auto read_func) { buffer.read_next(read_func); },
                     [&output_vector, &output_mx](int i) {
                       auto lock = std::lock_guard{output_mx};
                       output_vector.push_back(i);
                     }));
  }
  for (auto i = 0; i < n_values; ++i) {
    writers.push_back(std::jthread([this](int i) { buffer.write_next(i); }, i));
  }
  for (auto& r : readers) {
    r.join();
  }

  EXPECT_EQ(n_values, output_vector.size());
  std::sort(output_vector.begin(), output_vector.end());
  for (auto i = 0; auto const& x : output_vector) {
    EXPECT_EQ(i++, x);
  }
}
