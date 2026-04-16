cmake_minimum_required(VERSION 3.22)

project(timecode_ltc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(JUCE_SOURCE_DIR "" CACHE PATH "Path to a JUCE source checkout")

if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_plugin(TimecodeLTC
    BUNDLE_ID "com.timecodevst.ltcgenerator"
    COMPANY_NAME "timecode-vst"
    DESCRIPTION "Host-transport LTC generator"
    FORMATS AAX
    IS_MIDI_EFFECT FALSE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_CODE Ltc1
    PLUGIN_MANUFACTURER_CODE Tcod
    PLUGIN_NAME "LTC Generator"
    PRODUCT_NAME "LTC Generator"
    AAX_CATEGORY SWGenerators)

juce_generate_juce_header(TimecodeLTC)

target_sources(TimecodeLTC
    PRIVATE
        src/LtcGenerator.h
        src/PluginProcessor.cpp
        src/PluginProcessor.h)

target_compile_definitions(TimecodeLTC
    PUBLIC
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0)

target_link_libraries(TimecodeLTC
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
