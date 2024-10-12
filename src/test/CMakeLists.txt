add_executable(ThreadSafeBufferTest ThreadSafeBufferTest.cpp)
target_link_libraries(ThreadSafeBufferTest
  GTest::GTest
  GTest::Main
  ThreadSafeBuffer
)
target_include_directories(ThreadSafeBufferTest PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ThreadSafeBufferTest COMMAND ThreadSafeBufferTest)
