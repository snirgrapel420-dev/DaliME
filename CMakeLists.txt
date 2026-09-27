cmake_minimum_required(VERSION 3.22)
project(GoaMelodyEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GoaMelodyEngine
  COMPANY_NAME "GoaLab"
  PRODUCT_NAME "Goa Melody Engine"
  PLUGIN_MANUFACTURER_CODE Goal
  PLUGIN_CODE GmE1
  FORMATS VST3 AU Standalone
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_AUTO_MANIFEST FALSE
  VST3_CATEGORIES Instrument Generator
  AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(GoaMelodyEngine PRIVATE
  Source/Engine.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(GoaMelodyEngine PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(GoaMelodyEngine
  PRIVATE juce::juce_audio_utils juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(GOA_BUILD_TESTS "Build the engine unit test" OFF)
if(GOA_BUILD_TESTS)
  add_executable(engine_test tests/engine_test.cpp Source/Engine.cpp)
endif()
