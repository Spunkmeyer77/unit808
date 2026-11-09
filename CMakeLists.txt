cmake_minimum_required(VERSION 3.22)

project(UNIT808 VERSION 1.0.0)
string(TIMESTAMP U808_BUILD_DATE "%d %b %Y" UTC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE: use a local checkout if -DJUCE_DIR=... is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(U808_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(U808_FORMATS VST3 Standalone)
else()
    set(U808_FORMATS VST3 LV2 Standalone)
endif()

option(U808_COPY_PLUGINS "Copy plug-ins to the plug-in folders after building" ON)

juce_add_plugin(UNIT808
    COMPANY_NAME "Transistor Labs"
    PLUGIN_MANUFACTURER_CODE Trlb
    PLUGIN_CODE U808
    FORMATS ${U808_FORMATS}
    LV2URI "urn:transistorlabs:unit808"
    PRODUCT_NAME "UNIT808"
    BUNDLE_ID com.transistorlabs.unit808
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Drum
    COPY_PLUGIN_AFTER_BUILD ${U808_COPY_PLUGINS})

# panel artwork (rendered by tools/rasterise_ui.py) and the manual (opened from the About window)
file(GLOB U808_UI_PNGS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.png" "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.jpg")
juce_add_binary_data(UNIT808Data SOURCES "docs/UNIT808 Manual.pdf" ${U808_UI_PNGS})

set(U808_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/dsp/Tr808Engine.cpp)
target_sources(UNIT808 PRIVATE ${U808_SOURCES})

target_compile_definitions(UNIT808 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    _USE_MATH_DEFINES
    U808_BUILD_DATE="${U808_BUILD_DATE}")

target_link_libraries(UNIT808
    PRIVATE
        UNIT808Data
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: headless test app (sequencer, state, audio, editor snapshots)
option(U808_BUILD_TESTS "Build the plugin test app" OFF)
if(U808_BUILD_TESTS)
    juce_add_console_app(UNIT808Test PRODUCT_NAME "UNIT808Test")
    target_sources(UNIT808Test PRIVATE tools/plugin_test.cpp ${U808_SOURCES})
    target_compile_definitions(UNIT808Test PRIVATE
        JucePlugin_Name="UNIT808" JucePlugin_VersionString="${PROJECT_VERSION}" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        _USE_MATH_DEFINES U808_BUILD_DATE="${U808_BUILD_DATE}")
    target_link_libraries(UNIT808Test PRIVATE UNIT808Data juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
