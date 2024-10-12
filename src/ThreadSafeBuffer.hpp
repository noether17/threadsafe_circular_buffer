#pragma once

#include <array>
#include <atomic>
#include <iostream>
#include <sstream>
#include <thread>

#define LOGGING
#ifdef LOGGING
#define DEBUG_LOG(message)            \
  {                                   \
    std::stringstream output_stream;  \
    output_stream << message << '\n'; \
    std::cout << output_stream.str(); \
  }
#else
#define DEBUG_LOG(message)
#endif

template <typename T, int N>
class ThreadSafeBuffer {
 public:
  void write_next(T t) {
    DEBUG_LOG("Entered write_next().");
    int write_index = acquire_write_index();
    DEBUG_LOG("Acquired write index " << write_index);
    m_buffer[write_index] = std::move(t);
    release_write_index(write_index);
    DEBUG_LOG("Released write index " << write_index);
  }

  template <typename ReadFunc>
  void read_next(ReadFunc read_func) {
    DEBUG_LOG("Entered read_next().");
    int read_index = acquire_read_index();
    DEBUG_LOG("Acquired read index " << read_index);
    read_func(m_buffer[read_index]);
    release_read_index(read_index);
    DEBUG_LOG("Released read index " << read_index);
  }

 private:
  std::array<T, N> m_buffer{};
  std::atomic<int> m_next_write_index{};
  std::atomic<int> m_still_writing_index{};
  std::atomic<int> m_next_read_index{};
  std::atomic<int> m_still_reading_index{};
  std::atomic<bool> m_empty{true};

  int acquire_write_index() {
    int write_index = m_next_write_index.load();
    if (bool empty_expected = true;
        m_empty and m_empty.compare_exchange_strong(empty_expected, false)) {
      m_next_write_index.store(circular_increment(write_index));
      return write_index;
    }
    for (int trial = 0; write_index == m_still_reading_index.load() or
                        not m_next_write_index.compare_exchange_weak(
                            write_index, circular_increment(write_index));
         ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    return write_index;
  }

  void release_write_index(int write_index) {
    for (int trial = 0; m_still_writing_index.load() != write_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    m_still_writing_index.store(circular_increment(write_index));
  }

  int acquire_read_index() {
    int read_index = m_next_read_index.load();
    for (int trial = 0; read_index == m_still_writing_index.load() or
                        not m_next_read_index.compare_exchange_weak(
                            read_index, circular_increment(read_index));
         ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    return read_index;
  }

  void release_read_index(int read_index) {
    for (int trial = 0; m_still_reading_index.load() != read_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    auto next_read_index = circular_increment(read_index);
    m_still_reading_index.store(next_read_index);
    if (next_read_index == m_next_write_index.load()) {
      m_empty.store(true);
    }
  }

  auto static constexpr circular_increment(int i) {
    if constexpr ((N & (N - 1)) == 0) {  // if N is power of 2
      return (i + 1) % N;  // performed with bitwise-and mask for power of 2.
    } else {
      return ++i < N ? i : 0;
    }
  }
};
