cmake_minimum_required(VERSION 3.16)
project(nibble_encoder LANGUAGES CXX)

enable_testing()

add_executable(nibble_encoder_tests tests/nibble_encoder_tests.cpp)
target_compile_features(nibble_encoder_tests PRIVATE cxx_std_17)
target_include_directories(nibble_encoder_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME nibble_encoder_tests COMMAND nibble_encoder_tests)
