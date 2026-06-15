cmake_minimum_required(VERSION 3.16)
project(pypilot_client_protocol LANGUAGES CXX)

option(PYPILOT_CLIENT_PROTOCOL_BUILD_TESTS "Build tests" ON)
option(PYPILOT_CLIENT_PROTOCOL_BUILD_EXAMPLES "Build Linux examples" ON)

add_library(pypilot_client_protocol INTERFACE)
target_include_directories(pypilot_client_protocol INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(pypilot_client_protocol INTERFACE cxx_std_11)

if(PYPILOT_CLIENT_PROTOCOL_BUILD_EXAMPLES AND UNIX AND NOT APPLE)
  foreach(example pypilot-list-values pypilot-monitor pypilot-set)
    add_executable(${example} examples/linux/${example}.cpp)
    target_link_libraries(${example} PRIVATE pypilot_client_protocol)
    target_compile_definitions(${example} PRIVATE PYPILOT_CLIENT_PROTOCOL_ENABLE_LINUX_TCP)
  endforeach()
endif()

if(PYPILOT_CLIENT_PROTOCOL_BUILD_TESTS)
  enable_testing()
  add_executable(test_protocol tests/test_protocol.cpp)
  target_link_libraries(test_protocol PRIVATE pypilot_client_protocol)
  add_test(NAME test_protocol COMMAND test_protocol)
endif()
