cmake_minimum_required(VERSION 3.22)
project(StepFlux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
option(STEPFLUX_BUILD_TESTS "Build the console test runner" OFF)

if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(STEPFLUX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND STEPFLUX_FORMATS AU)
endif()

juce_add_plugin(StepFlux
    COMPANY_NAME "LionAudio"
    BUNDLE_ID "com.lionaudio.stepflux"
    PLUGIN_MANUFACTURER_CODE Lion
    PLUGIN_CODE Stfx
    FORMATS ${STEPFLUX_FORMATS}
    PRODUCT_NAME "StepFlux"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(StepFlux PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(StepFlux PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(StepFlux
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

if(STEPFLUX_BUILD_TESTS)
    juce_add_console_app(StepFluxTests PRODUCT_NAME "StepFluxTests")
    target_sources(StepFluxTests PRIVATE Tests/Tests.cpp)
    target_include_directories(StepFluxTests PRIVATE Source)
    target_link_libraries(StepFluxTests PRIVATE StepFlux juce::juce_audio_utils)
endif()
