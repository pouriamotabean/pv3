cmake_minimum_required(VERSION 3.22)
project(PV VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE is downloaded by the GitHub Actions workflow.
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "JUCE source directory")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(PV
    COMPANY_NAME "PV Audio"
    BUNDLE_ID "com.pvaudio.PV"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE PVAU
    PLUGIN_CODE PV01
    FORMATS VST3
    PRODUCT_NAME "PV"
)

target_sources(PV PRIVATE
    src/PVProcessor.cpp
    src/PVEditor.cpp
)

target_compile_definitions(PV PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PV PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
)
