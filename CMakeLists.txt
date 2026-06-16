cmake_minimum_required(VERSION 3.22)

project(BitTrixer VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8: use a local checkout if RF_JUCE_PATH is set, otherwise fetch it.
set(RF_JUCE_PATH "" CACHE PATH
    "Path to a local JUCE 8 checkout (leave empty to download JUCE automatically)")
set(RF_JUCE_TAG "8.0.4" CACHE STRING
    "JUCE git tag/branch to fetch when RF_JUCE_PATH is empty")

if(RF_JUCE_PATH)
    add_subdirectory("${RF_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        "${RF_JUCE_TAG}"
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(BitTrixer
    COMPANY_NAME              "Bytemixer"
    PRODUCT_NAME              "BitTrixer"
    PLUGIN_MANUFACTURER_CODE  Btmx
    PLUGIN_CODE               Btrx
    FORMATS                   VST3 Standalone
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE)

target_sources(BitTrixer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Randomizer.cpp
    Source/DSP/SynthEngine.cpp
    Source/UI/RetroLookAndFeel.cpp)

target_compile_definitions(BitTrixer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(BitTrixer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
