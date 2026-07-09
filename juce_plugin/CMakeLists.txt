cmake_minimum_required(VERSION 3.20)
project(PRA32ColorcoderPlugin VERSION 1.0.0)

# Fetch JUCE if not present locally
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
)
FetchContent_MakeAvailable(juce)

# Create a JUCE plugin target
juce_add_plugin(PRA32ColorcoderPlugin
    COMPANY_NAME "PRA32"
    PLUGIN_MANUFACTURER_CODE "PR32"
    PLUGIN_CODE "Colr"
    FORMATS VST3 Standalone
    PRODUCT_NAME "PRA32 Colorcoder"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

# Source files
target_sources(PRA32ColorcoderPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PRA32LookAndFeel.cpp
    Source/PRA32LookAndFeel.h
    Source/SynthParameters.cpp
    Source/SynthParameters.h
)

# Include directories: the synth engine and the web_app dummy headers
target_include_directories(PRA32ColorcoderPlugin PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../Digital-Synth-PRA32-U2"
    "${CMAKE_CURRENT_SOURCE_DIR}/../web_app"
)

# Link to JUCE modules
target_link_libraries(PRA32ColorcoderPlugin PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# C++20 for JUCE 8 compatibility
target_compile_features(PRA32ColorcoderPlugin PUBLIC cxx_std_20)
