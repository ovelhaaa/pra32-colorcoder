cmake_minimum_required(VERSION 3.20)

# Standalone tests for the JUCE/VST wrapper. These do not require JUCE: the
# engine tests compile the real embedded engine against the desktop shims used
# by the web build, and the logic tests cover the shared parameter metadata.
project(PRA32ColorcoderTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(pra32_param_logic_tests param_logic_tests.cpp)
target_include_directories(pra32_param_logic_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source")

add_executable(pra32_engine_tests engine_tests.cpp)
target_compile_definitions(pra32_engine_tests PRIVATE
    PRA32_U2_ENABLE_POLY_ON_1_CORE=1
    PRA32_U2_USE_EMULATED_EEPROM=1)
target_include_directories(pra32_engine_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../web_app"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Digital-Synth-PRA32-U2")

if (MSVC)
    target_compile_options(pra32_engine_tests PRIVATE /W3)
else()
    target_compile_options(pra32_engine_tests PRIVATE -Wall -Wextra)
    target_compile_options(pra32_param_logic_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME param_logic COMMAND pra32_param_logic_tests)
add_test(NAME engine      COMMAND pra32_engine_tests)
