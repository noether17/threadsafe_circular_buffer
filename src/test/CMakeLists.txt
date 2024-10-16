add_executable(ThreadSafeBufferTest ThreadSafeBufferTest.cpp)
target_link_libraries(ThreadSafeBufferTest
  GTest::GTest
  GTest::Main
  ThreadSafeBuffer
)
target_include_directories(ThreadSafeBufferTest PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ThreadSafeBufferTest COMMAND ThreadSafeBufferTest)

add_executable(ThreadSafeBuffer2Test ThreadSafeBuffer2Test.cpp)
target_link_libraries(ThreadSafeBuffer2Test
  GTest::GTest
  GTest::Main
  ThreadSafeBuffer
)
target_include_directories(ThreadSafeBuffer2Test PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME ThreadSafeBuffer2Test COMMAND ThreadSafeBuffer2Test)
