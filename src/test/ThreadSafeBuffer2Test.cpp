#include <gtest/gtest.h>

#include <algorithm>
#include <mutex>
#include <thread>
#include <vector>

#include "ThreadSafeBuffer2.hpp"

class ThreadSafeBuffer2Test : public testing::Test {
 protected:
  auto static constexpr buffer_size = 16;
  auto static constexpr n_passes = 1024;
  auto static constexpr n_values = n_passes * buffer_size;

  auto static constexpr n_threads = 16;
  auto static constexpr n_ops_per_thread = n_values / n_threads;

  ThreadSafeBuffer2<int, buffer_size> buffer{};
};

TEST_F(ThreadSafeBuffer2Test, SingleThreadAlternateWriteRead) {
  auto output_vector = std::vector<int>{};

  for (auto i = 0; i < n_values; ++i) {
    buffer.write_next(i);
    buffer.read_next([&output_vector](int a) { output_vector.push_back(a); });
  }

  EXPECT_EQ(n_values, output_vector.size());
  for (auto i = 0; i < n_values; ++i) {
    EXPECT_EQ(i, output_vector[i]);
  }
}

TEST_F(ThreadSafeBuffer2Test, MultipleWritersSingleReader) {
  auto writers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};

  for (auto i = 0; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  for (auto i = 0; i < n_values; ++i) {
    buffer.read_next([&output_vector](int a) { output_vector.push_back(a); });
  }

  EXPECT_EQ(n_values, output_vector.size());
  std::sort(output_vector.begin(), output_vector.end());
  for (auto i = 0; auto const& x : output_vector) {
    EXPECT_EQ(i++, x);
  }
}

TEST_F(ThreadSafeBuffer2Test, SingleWriterMultipleReaders) {
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  // need to start readers before writing on main thread
  for (auto i = 0; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          output_vector.push_back(a);
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

TEST_F(ThreadSafeBuffer2Test, MultipleWritersMultipleReadersWriteFirst) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  for (auto i = 0; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          output_vector.push_back(a);
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

TEST_F(ThreadSafeBuffer2Test, MultipleWritersMultipleReadersReadFirst) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          output_vector.push_back(a);
        }));
  }
  for (auto i = 0; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.write_next(thread_offset + j);
          }
        },
        i));
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

TEST_F(ThreadSafeBuffer2Test, MultipleWritersMultipleReadersSlowWrites) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1us);
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  for (auto i = 0; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          output_vector.push_back(a);
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

TEST_F(ThreadSafeBuffer2Test, MultipleWritersMultipleReadersSlowReads) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  for (auto i = 0; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  for (auto i = 0; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(1us);
          output_vector.push_back(a);
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

TEST_F(ThreadSafeBuffer2Test, MultipleWritersMultipleReadersMixedSpeeds) {
  auto writers = std::vector<std::jthread>{};
  auto readers = std::vector<std::jthread>{};
  auto output_vector = std::vector<int>{};
  auto output_mx = std::mutex{};

  // slow writes
  for (auto i = 0; i < n_threads / 2; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1us);
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  // fast writes
  for (auto i = n_threads / 2; i < n_threads; ++i) {
    writers.push_back(std::jthread(
        [this](int i) {
          auto thread_offset = i * n_ops_per_thread;
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.write_next(thread_offset + j);
          }
        },
        i));
  }
  // slow reads
  for (auto i = 0; i < n_threads / 2; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(1us);
          output_vector.push_back(a);
        }));
  }
  // fast reads
  for (auto i = n_threads / 2; i < n_threads; ++i) {
    readers.push_back(std::jthread(
        [this](auto read_func) {
          for (auto j = 0; j < n_ops_per_thread; ++j) {
            buffer.read_next(read_func);
          }
        },
        [&output_vector, &output_mx](int a) {
          auto lock = std::lock_guard{output_mx};
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(1us);
          output_vector.push_back(a);
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
