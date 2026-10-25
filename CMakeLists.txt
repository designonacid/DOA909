cmake_minimum_required(VERSION 3.22)
project(Thump VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# Use a local JUCE checkout if given (-DJUCE_DIR=/path/to/JUCE), otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(THUMP_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND THUMP_FORMATS AU)
endif()

juce_add_plugin(Thump
    COMPANY_NAME             "AcidLab"
    PRODUCT_NAME             "Thump"
    BUNDLE_ID                "com.acidlab.thump"
    PLUGIN_MANUFACTURER_CODE Acdl
    PLUGIN_CODE              Thmp
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES          Instrument Drum
    AU_MAIN_TYPE             kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD  FALSE
    FORMATS                  ${THUMP_FORMATS})

target_sources(Thump PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/CurveEditor.cpp)

target_include_directories(Thump PRIVATE Source)

target_compile_definitions(Thump PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Thump
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless test: renders audio through the full plugin and snapshots the UI
option(THUMP_BUILD_TESTS "Build the plugin smoke test" OFF)
if(THUMP_BUILD_TESTS)
    add_executable(ThumpSmokeTest tests/smoke_test.cpp)
    target_include_directories(ThumpSmokeTest PRIVATE Source $<TARGET_PROPERTY:Thump,INCLUDE_DIRECTORIES>)
    target_compile_definitions(ThumpSmokeTest PRIVATE $<TARGET_PROPERTY:Thump,COMPILE_DEFINITIONS>)
    target_link_libraries(ThumpSmokeTest PRIVATE Thump juce::juce_recommended_config_flags)
endif()
