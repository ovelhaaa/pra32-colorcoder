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
    COMPANY_NAME "Tonecoder"
    PLUGIN_MANUFACTURER_CODE "PR32"
    PLUGIN_CODE "Colr"
    FORMATS VST3 Standalone
    PRODUCT_NAME "Tonecoder TC-32"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CAN_REPLACE_VST2 FALSE
)

# Generate JuceHeader.h
juce_generate_juce_header(PRA32ColorcoderPlugin)

# Source files
target_sources(PRA32ColorcoderPlugin PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PRA32LookAndFeel.cpp
    Source/PRA32LookAndFeel.h
    Source/PRA32Theme.cpp
    Source/PRA32Theme.h
    Source/PRA32ValueFormatter.cpp
    Source/PRA32ValueFormatter.h
    Source/PRA32Components.cpp
    Source/PRA32Components.h
    Source/PRA32ParamLogic.h
    Source/FactoryPresets.h
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

# Forcefully disable VST2 compatibility in VST3 (fixes missing vstfxstore.h)
target_compile_definitions(PRA32ColorcoderPlugin PUBLIC JUCE_VST3_CAN_REPLACE_VST2=0)

# The PRA32-U2 engine only renders the extra voices when one of its polyphony
# modes is compiled in. On desktop there is a single audio thread, so enable the
# single-core polyphonic path (4 voices). Without this the synth is monophonic
# regardless of the VOICE_MODE parameter.
target_compile_definitions(PRA32ColorcoderPlugin PRIVATE PRA32_U2_ENABLE_POLY_ON_1_CORE=1)

# Standalone, JUCE-free regression tests (engine + parameter logic).
option(PRA32_BUILD_TESTS "Build the standalone PRA32 wrapper tests" OFF)
if(PRA32_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
