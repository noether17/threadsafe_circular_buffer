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
class ThreadSafeBuffer2 {
 public:
  void write_next(T t) {
    DEBUG_LOG("Entered write_next().");
    auto write_index = acquire_write_index();
    m_buffer[write_index % N] = std::move(t);
    release_write_index(write_index);
  }

  template <typename ReadFunc>
  void read_next(ReadFunc read_func) {
    DEBUG_LOG("Entered read_next().");
    auto read_index = acquire_read_index();
    read_func(m_buffer[read_index % N]);
    release_read_index(read_index);
  }

 private:
  std::array<T, N> m_buffer{};
  std::atomic<unsigned int> m_next_write_index{};
  std::atomic<unsigned int> m_still_writing_index{};
  std::atomic<unsigned int> m_next_read_index{};
  std::atomic<unsigned int> m_still_reading_index{};

  int acquire_write_index() {
    auto write_index = m_next_write_index.load();
    DEBUG_LOG("Attempting to acquire write index " << write_index << " ("
                                                   << write_index % N << ")"
                                                   << output_state());
    for (auto trial = 0; not(((write_index = m_next_write_index.load()) !=
                              m_still_reading_index.load() + N) and
                             m_next_write_index.compare_exchange_strong(
                                 write_index, write_index + 1));
         ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    DEBUG_LOG("Acquired write index " << write_index << " (" << write_index % N
                                      << ")");
    return write_index;
  }

  void release_write_index(unsigned int write_index) {
    DEBUG_LOG("Entering release_write_index() with write index "
              << write_index << " (" << write_index % N << ")"
              << output_state());
    for (int trial = 0; m_still_writing_index.load() != write_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    m_still_writing_index.fetch_add(1u);
    DEBUG_LOG("Released write index " << write_index << " (" << write_index % N
                                      << ")");
  }

  int acquire_read_index() {
    auto read_index = m_next_read_index.load();
    DEBUG_LOG("Attempting to acquire read index "
              << read_index << " (" << read_index % N << ")" << output_state());
    for (int trial = 0; not(((read_index = m_next_read_index.load()) !=
                             m_still_writing_index.load()) and
                            m_next_read_index.compare_exchange_strong(
                                read_index, read_index + 1));
         ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    DEBUG_LOG("Acquired read index " << read_index << " (" << read_index % N
                                     << ")");
    return read_index;
  }

  void release_read_index(unsigned int read_index) {
    DEBUG_LOG("Entering release_read_index() with read index "
              << read_index << " (" << read_index % N << ")" << output_state());
    for (int trial = 0; m_still_reading_index.load() != read_index; ++trial) {
      // spinlock
      if (trial == 8) {
        trial = 0;
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ns);
      }
    }
    m_still_reading_index.fetch_add(1u);
    DEBUG_LOG("Released read index " << read_index << " (" << read_index % N
                                     << ")");
  }

  auto output_state() {
    auto next_write = m_next_write_index.load();
    auto still_writing = m_still_writing_index.load();
    auto next_read = m_next_read_index.load();
    auto still_reading = m_still_reading_index.load();

    auto state_str = std::string{"; Current state:"};
    state_str += std::string{" nw="} + std::to_string(next_write);
    state_str += std::string{" sw="} + std::to_string(still_writing);
    state_str += std::string{" nr="} + std::to_string(next_read);
    state_str += std::string{" sr="} + std::to_string(still_reading);

    return state_str;
  }
};
