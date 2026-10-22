# rabcl_esp32 のホスト側ユニットテスト
#   cmake -S test -B build && cmake --build build && ctest --test-dir build --output-on-failure
cmake_minimum_required(VERSION 3.16)
project(rabcl_esp32_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(RABCL_ESP32_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

add_executable(rabcl_esp32_test
  ${RABCL_ESP32_DIR}/src/can_bus.cpp
  ${RABCL_ESP32_DIR}/src/debug_led.cpp
  fakes/fake_state.cpp
  test_can_bus.cpp
  test_debug_led.cpp
)
# fakes を先頭に置き、本物の ESP-IDF ヘッダの代わりに差し替える
target_include_directories(rabcl_esp32_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/fakes
  ${RABCL_ESP32_DIR}/include
)
target_link_libraries(rabcl_esp32_test PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(rabcl_esp32_test)
