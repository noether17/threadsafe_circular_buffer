#pragma once

#include <array>
#include <atomic>
#include <string>
#include <thread>

// #define LOGGING
#ifdef LOGGING
#include <iostream>
#include <sstream>
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
    m_buffer[write_index] = std::move(t);
    release_write_index(write_index);
  }

  template <typename ReadFunc>
  void read_next(ReadFunc read_func) {
    DEBUG_LOG("Entered read_next().");
    int read_index = acquire_read_index();
    read_func(m_buffer[read_index]);
    release_read_index(read_index);
  }

 private:
  std::array<T, N> m_buffer{};
  std::atomic<int> m_next_write_index{};
  std::atomic<int> m_still_writing_index{};
  std::atomic<int> m_next_read_index{};
  std::atomic<int> m_still_reading_index{};
  std::atomic<bool> m_empty{true};
  std::atomic<bool> m_full{false};

  int acquire_write_index() {
    int write_index = m_next_write_index.load();
    DEBUG_LOG("Attempting to acquire write index " << write_index
                                                   << output_state());
    bool expect_true = true;
    for (int trial = 0;
         not(((m_empty.load() and
               m_empty.compare_exchange_strong(expect_true, false)) or
              (write_index = m_next_write_index.load()) !=
                  m_still_reading_index.load()) and
             m_next_write_index.compare_exchange_strong(
                 write_index, circular_increment(write_index)));
         ++trial, expect_true = true) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    DEBUG_LOG("Acquired write index " << write_index);
    return write_index;
  }

  void release_write_index(int write_index) {
    DEBUG_LOG("Entering release_write_index() with write index "
              << write_index << output_state());
    for (int trial = 0; m_still_writing_index.load() != write_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    auto new_still_writing_index = circular_increment(write_index);
    m_still_writing_index.store(new_still_writing_index);
    if (new_still_writing_index == m_next_read_index.load()) {
      m_full.store(true);
      DEBUG_LOG("Filled the buffer at write index " << write_index);
    }
    DEBUG_LOG("Released write index " << write_index);
  }

  int acquire_read_index() {
    int read_index = m_next_read_index.load();
    DEBUG_LOG("Attempting to acquire read index " << read_index
                                                  << output_state());
    bool expect_true = true;
    for (int trial = 0; not(((m_full.load() and m_full.compare_exchange_strong(
                                                    expect_true, false)) or
                             (read_index = m_next_read_index.load()) !=
                                 m_still_writing_index.load()) and
                            m_next_read_index.compare_exchange_strong(
                                read_index, circular_increment(read_index)));
         ++trial, expect_true = true) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    DEBUG_LOG("Acquired read index " << read_index);
    return read_index;
  }

  void release_read_index(int read_index) {
    DEBUG_LOG("Entering release_read_index() with read index "
              << read_index << output_state());
    for (int trial = 0; m_still_reading_index.load() != read_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    auto new_still_reading_index = circular_increment(read_index);
    m_still_reading_index.store(new_still_reading_index);
    if (new_still_reading_index == m_next_write_index.load()) {
      m_empty.store(true);
      DEBUG_LOG("Emptied the buffer at read index " << read_index);
    }
    DEBUG_LOG("Released read index " << read_index);
  }

  auto output_state() {
    auto next_write = m_next_write_index.load();
    auto still_writing = m_still_writing_index.load();
    auto next_read = m_next_read_index.load();
    auto still_reading = m_still_reading_index.load();
    auto empty = m_empty.load();
    auto full = m_full.load();

    auto state_str = std::string{"; Current state:"};
    state_str += std::string{" nw="} + std::to_string(next_write);
    state_str += std::string{" sw="} + std::to_string(still_writing);
    state_str += std::string{" nr="} + std::to_string(next_read);
    state_str += std::string{" sr="} + std::to_string(still_reading);
    state_str += std::string{" e="} + std::to_string(empty);
    state_str += std::string{" f="} + std::to_string(full);

    return state_str;
  }

  // TODO: Keeping indices in the range [0, N) ensures correctness of the
  // increment operation for N not a power of 2, since integer overflow would
  // cause an error in such cases. However, in a multithreaded setting, wrapping
  // the indices allows multiple threads to access the same index
  // simultaneously, since there is no distinction between subsequent passes
  // through the buffer. To ensure correctness, the index needs to not only be
  // kept in the range [0, N), but also tagged with the pass number so that
  // indices referring to the same location but from different passes do not
  // compare equal.
  auto static constexpr circular_increment(int i) {
    if constexpr ((N & (N - 1)) == 0) {  // if N is power of 2
      return (i + 1) % N;  // performed with bitwise-and mask for power of 2.
    } else {
      return ++i < N ? i : 0;
    }
  }
};
