cmake_minimum_required(VERSION 3.16)
project(modbus_codec VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- library --------------------------------------------------------------
add_library(modbus_codec src/modbus.cpp)
target_include_directories(modbus_codec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
  target_compile_options(modbus_codec PRIVATE /W4)
else()
  target_compile_options(modbus_codec PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- CLI ------------------------------------------------------------------
add_executable(modbus_cli apps/modbus_cli.cpp)
set_target_properties(modbus_cli PROPERTIES OUTPUT_NAME modbus)
target_link_libraries(modbus_cli PRIVATE modbus_codec)

# ---- tests ----------------------------------------------------------------
option(MODBUS_BUILD_TESTS "Build unit tests" ON)
if(MODBUS_BUILD_TESTS)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(modbus_tests tests/modbus_tests.cpp)
  target_link_libraries(modbus_tests PRIVATE modbus_codec Catch2::Catch2WithMain)

  enable_testing()
  add_test(NAME unit COMMAND modbus_tests)
endif()
