cmake_minimum_required(VERSION 3.22)
project(LevelNativosAudio VERSION 0.2.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
if(APPLE)
  set(LEVEL_FORMATS VST3 AU)
else()
  set(LEVEL_FORMATS VST3)
endif()
juce_add_plugin(Level COMPANY_NAME "Nativos Audio" BUNDLE_ID "com.nativosaudio.level"
  PLUGIN_MANUFACTURER_CODE NAtv PLUGIN_CODE LvL1 FORMATS ${LEVEL_FORMATS}
  PRODUCT_NAME "Level" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(Level PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(Level PRIVATE Source)
target_compile_definitions(Level PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(Level PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_gui_extra
juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
