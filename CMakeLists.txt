cmake_minimum_required(VERSION 3.22)
project(GRANULAR_SYSTEM VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary (Apple silicon + Intel)")
endif()

# JUCE: point GS_JUCE_DIR at a local checkout, or it is downloaded automatically.
set(GS_JUCE_DIR "" CACHE PATH "Path to an existing JUCE checkout (optional)")
if(GS_JUCE_DIR)
    add_subdirectory(${GS_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.6 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(GS_DEFAULT_FORMATS AU VST3 Standalone)
else()
    set(GS_DEFAULT_FORMATS VST3 Standalone)
endif()
set(GS_FORMATS ${GS_DEFAULT_FORMATS} CACHE STRING "Plugin formats to build")

juce_add_plugin(GranularSystem
    PRODUCT_NAME "GRANULAR_SYSTEM"
    COMPANY_NAME "GRANULAR_SYSTEM"
    BUNDLE_ID "com.granularsystem.plugin"
    PLUGIN_MANUFACTURER_CODE Grsy
    PLUGIN_CODE Gsy1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS ${GS_FORMATS})

target_sources(GranularSystem PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(GranularSystem PRIVATE Source)
target_compile_definitions(GranularSystem PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(GranularSystem
    PRIVATE juce::juce_audio_utils juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Optional headless test host (used during development; not needed to build the plugin).
option(GS_BUILD_TESTS "Build the headless host test" OFF)
if(GS_BUILD_TESTS)
    add_executable(gs_host_test tests/host_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(gs_host_test PRIVATE Source)
    target_compile_definitions(gs_host_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(gs_host_test PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
