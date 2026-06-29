cmake_minimum_required(VERSION 3.16)
project(pypilot_client_protocol LANGUAGES CXX)

option(PYPILOT_CLIENT_PROTOCOL_BUILD_TESTS "Build tests" ON)
option(PYPILOT_CLIENT_PROTOCOL_FETCH_ARDUINOJSON "Fetch ArduinoJson for non-Arduino builds" ON)
set(PYPILOT_CLIENT_PROTOCOL_ARDUINOJSON_GIT_TAG "v7.4.2" CACHE STRING "ArduinoJson git tag used by FetchContent")

add_library(pypilot_client_protocol INTERFACE)
target_include_directories(pypilot_client_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(pypilot_client_protocol INTERFACE cxx_std_11)

if(PYPILOT_CLIENT_PROTOCOL_FETCH_ARDUINOJSON)
  include(FetchContent)
  FetchContent_Declare(
    arduinojson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG ${PYPILOT_CLIENT_PROTOCOL_ARDUINOJSON_GIT_TAG}
  )
  FetchContent_GetProperties(arduinojson)
  if(NOT arduinojson_POPULATED)
    FetchContent_Populate(arduinojson)
  endif()
  target_include_directories(pypilot_client_protocol INTERFACE ${arduinojson_SOURCE_DIR}/src)
endif()

if(PYPILOT_CLIENT_PROTOCOL_BUILD_TESTS)
  enable_testing()
  add_executable(test_protocol tests/test_protocol.cpp)
  target_link_libraries(test_protocol PRIVATE pypilot_client_protocol)
  add_test(NAME test_protocol COMMAND test_protocol)
endif()
