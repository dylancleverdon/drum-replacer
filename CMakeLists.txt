cmake_minimum_required(VERSION 3.22)

# VERSION holds major.minor. CI picks the patch number for each release and passes the
# full version in; local builds are 0-patch "-dev" builds.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" KILLROOM_BASE_VERSION LIMIT_COUNT 1)
set(KILLROOM_VERSION "${KILLROOM_BASE_VERSION}.0" CACHE STRING "Numeric plugin version (major.minor.patch)")
set(KILLROOM_VERSION_LABEL "${KILLROOM_VERSION}-dev" CACHE STRING "Version shown in the plugin and used by the updater")
set(KILLROOM_GITHUB_REPO "dylancleverdon/drum-replacer" CACHE STRING "GitHub repo (owner/name) that publishes releases")

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
# Static MSVC runtime, so the plugin doesn't depend on a Visual C++ redistributable being installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Killroom VERSION ${KILLROOM_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(KILLROOM_BUILD_TESTS "Build the DSP tests" ON)
option(KILLROOM_BUILD_SNAPSHOT "Build the UI snapshot tool (development only)" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# The IDs below identify the plugin to Live. Changing PLUGIN_MANUFACTURER_CODE or
# PLUGIN_CODE turns it into a different plugin and breaks every saved set that uses it.
juce_add_plugin(Killroom
    PRODUCT_NAME "Killroom"
    COMPANY_NAME "dylancleverdon"
    BUNDLE_ID "com.dylancleverdon.killroom"
    PLUGIN_MANUFACTURER_CODE Dcln
    PLUGIN_CODE Kilr
    FORMATS VST3
    VST3_CATEGORIES Fx Dynamics
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    NEEDS_CURL TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(KillroomData
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        scripts/install-mac.sh
        scripts/install-windows.ps1)

set(killroom_sources
    Source/dsp/Killroom.cpp
    Source/Parameters.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp
    Source/Ui.cpp
    Source/UpdateService.cpp)

target_sources(Killroom PRIVATE ${killroom_sources})

target_compile_definitions(Killroom PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    KILLROOM_VERSION_LABEL="${KILLROOM_VERSION_LABEL}"
    KILLROOM_GITHUB_REPO="${KILLROOM_GITHUB_REPO}")

target_link_libraries(Killroom
    PRIVATE
        KillroomData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(KILLROOM_BUILD_TESTS)
    enable_testing()
    add_executable(KillroomDspTests tests/DspTests.cpp Source/dsp/Killroom.cpp)
    target_compile_features(KillroomDspTests PRIVATE cxx_std_17)
    add_test(NAME dsp COMMAND KillroomDspTests)

    juce_add_console_app(KillroomUpdateTests PRODUCT_NAME "KillroomUpdateTests")
    target_sources(KillroomUpdateTests PRIVATE tests/UpdateTests.cpp Source/UpdateService.cpp)
    target_compile_definitions(KillroomUpdateTests PRIVATE
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        KILLROOM_VERSION_LABEL="1.0.5"
        KILLROOM_GITHUB_REPO="${KILLROOM_GITHUB_REPO}")
    target_link_libraries(KillroomUpdateTests PRIVATE
        KillroomData
        juce::juce_events
        juce::juce_recommended_config_flags)
    add_test(NAME update COMMAND KillroomUpdateTests)
endif()

if(KILLROOM_BUILD_SNAPSHOT)
    # Renders the editor to a PNG without a DAW: KillroomSnapshot out.png
    juce_add_console_app(KillroomSnapshot PRODUCT_NAME "KillroomSnapshot" NEEDS_CURL TRUE)
    target_sources(KillroomSnapshot PRIVATE tools/Snapshot.cpp ${killroom_sources})
    target_compile_definitions(KillroomSnapshot PRIVATE
        JucePlugin_Name="Killroom"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_MODAL_LOOPS_PERMITTED=1
        KILLROOM_VERSION_LABEL="${KILLROOM_VERSION_LABEL}"
        KILLROOM_GITHUB_REPO="${KILLROOM_GITHUB_REPO}")
    target_link_libraries(KillroomSnapshot PRIVATE
        KillroomData
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
endif()
