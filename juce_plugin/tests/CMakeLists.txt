cmake_minimum_required(VERSION 3.20)

# Tests for the JUCE/VST wrapper.
#
# Most of these are JUCE-free: the engine tests compile the real embedded engine
# against the desktop shims used by the web build, and the logic/midi/resampler
# tests cover framework-free behaviour. The optional plugin_tests target (added
# at the bottom) does use JUCE for APVTS state/preset roundtrips.
project(PRA32ColorcoderTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(Threads REQUIRED)

add_executable(pra32_param_logic_tests param_logic_tests.cpp)
target_include_directories(pra32_param_logic_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source")

add_executable(pra32_midi_state_tests midi_state_tests.cpp)
target_include_directories(pra32_midi_state_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source")
target_link_libraries(pra32_midi_state_tests PRIVATE Threads::Threads)

add_executable(pra32_engine_tests engine_tests.cpp)
target_compile_definitions(pra32_engine_tests PRIVATE
    PRA32_U2_ENABLE_POLY_ON_1_CORE=1
    PRA32_U2_USE_EMULATED_EEPROM=1)
target_include_directories(pra32_engine_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../web_app"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Digital-Synth-PRA32-U2")

# Milestone B: the exact resampler the JUCE wrapper uses, driven by the real
# engine, is regression tested standalone (no JUCE required).
add_executable(pra32_resampler_tests resampler_tests.cpp)
target_compile_definitions(pra32_resampler_tests PRIVATE
    PRA32_U2_ENABLE_POLY_ON_1_CORE=1
    PRA32_U2_USE_EMULATED_EEPROM=1)
target_include_directories(pra32_resampler_tests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../web_app"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Digital-Synth-PRA32-U2")

if (MSVC)
    target_compile_options(pra32_engine_tests PRIVATE /W3)
    target_compile_options(pra32_resampler_tests PRIVATE /W3)
else()
    target_compile_options(pra32_engine_tests PRIVATE -Wall -Wextra)
    target_compile_options(pra32_param_logic_tests PRIVATE -Wall -Wextra)
    target_compile_options(pra32_midi_state_tests PRIVATE -Wall -Wextra)
    target_compile_options(pra32_resampler_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME param_logic COMMAND pra32_param_logic_tests)
add_test(NAME midi_state  COMMAND pra32_midi_state_tests)
add_test(NAME engine      COMMAND pra32_engine_tests)
add_test(NAME resampler   COMMAND pra32_resampler_tests)

# -----------------------------------------------------------------------------
# Milestone B: JUCE plugin-level tests (state/preset roundtrip, automation).
#
# Only built when this directory is added from the top-level juce_plugin/CMake
# (where the JUCE target and its helper functions exist). Reuses the exact plugin
# sources so the tests exercise the shipping implementation.
# -----------------------------------------------------------------------------
if (TARGET juce::juce_audio_processors)
    juce_add_console_app(pra32_plugin_tests PRODUCT_NAME "PRA32 Plugin Tests")

    target_sources(pra32_plugin_tests PRIVATE
        plugin_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PluginProcessor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PluginEditor.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PRA32LookAndFeel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PRA32Theme.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PRA32ValueFormatter.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/PRA32Components.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source/SynthParameters.cpp"
    )

    # The plugin source uses the JucePlugin_* macros normally supplied by
    # juce_add_plugin; provide the ones it reads when built as a console app.
    target_compile_definitions(pra32_plugin_tests PRIVATE
        JUCE_VST3_CAN_REPLACE_VST2=0
        PRA32_U2_ENABLE_POLY_ON_1_CORE=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_IsSynth=1
        "JucePlugin_Name=\"Tonecoder TC-32\""
    )

    target_include_directories(pra32_plugin_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../Source"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../web_app"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Digital-Synth-PRA32-U2"
    )

    target_link_libraries(pra32_plugin_tests PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )

    target_compile_features(pra32_plugin_tests PRIVATE cxx_std_20)

    # Must follow target_link_libraries so the generated header sees the linked
    # JUCE modules (matches JUCE's console-app examples).
    juce_generate_juce_header(pra32_plugin_tests)

    add_test(NAME plugin COMMAND pra32_plugin_tests)
endif()
