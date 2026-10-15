cmake_minimum_required(VERSION 3.22)

# A version bump makes Logic re-validate the AU; bump it with every change
# that should reach the host.
project(FMBox VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "26.5" CACHE STRING "" FORCE)

set(JUCE_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to the JUCE framework")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Pass -DJUCE_PATH=/path/to/JUCE.")
endif()

add_subdirectory("${JUCE_PATH}" JUCE)

# ------------------------------------------------------------------------------
# DaisySP - the vendored drum models (MIT), built on their own so JUCE's
# warning flags do not apply to code that is not ours to restyle.
# ------------------------------------------------------------------------------

set(DAISYSP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source/ThirdParty/DaisySP")

add_library(daisysp STATIC
    ${DAISYSP_DIR}/Filters/svf.cpp
    ${DAISYSP_DIR}/Synthesis/oscillator.cpp
    ${DAISYSP_DIR}/Drums/synthbassdrum.cpp
    ${DAISYSP_DIR}/Drums/synthsnaredrum.cpp
    ${DAISYSP_DIR}/Drums/hihat.cpp)

# SYSTEM: its headers are included from FMBox's sources, and JUCE's warning
# flags would otherwise report DaisySP's style as FMBox's warnings.
target_include_directories(daisysp SYSTEM PUBLIC
    ${DAISYSP_DIR}
    ${DAISYSP_DIR}/Utility
    ${DAISYSP_DIR}/Filters
    ${DAISYSP_DIR}/Synthesis)

target_compile_options(daisysp PRIVATE -O2 -w)
set_target_properties(daisysp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ------------------------------------------------------------------------------
# The plugin
# ------------------------------------------------------------------------------

juce_add_plugin(FMBox
    # The vendor a host lists this under - display only. The identity is the
    # pair of four-character codes below: the AU is aumu/Fmbx/Tzor and the VST3
    # class id is a hash of the same two codes. Changing either after a session
    # has been saved makes a different plugin, and that session loses it.
    COMPANY_NAME                "T'Zorr"
    BUNDLE_ID                   TZorr.FMBox
    PLUGIN_MANUFACTURER_CODE    Tzor
    PLUGIN_CODE                 Fmbx
    FORMATS                     AU VST3 Standalone
    PRODUCT_NAME                "FMBox"
    DESCRIPTION                 "8-sound FM drum synth"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE    # Scripts/install.sh does this, cache flush and all
    VST3_CATEGORIES             Instrument Drum Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    HARDENED_RUNTIME_ENABLED    TRUE
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE)

# The engine is listed separately so the verification target links exactly the
# objects the plugin runs. Testing a reimplementation tests nothing.
set(FMBOX_CORE_SOURCES
    Source/Engine/FmVoices.cpp
    Source/Engine/FmDrumEngine.cpp
    Source/Engine/Fx/PlateReverb.cpp
    Source/Engine/Fx/EffectCatalog.cpp
    Source/Engine/Fx/EffectBlock.cpp)

set(FMBOX_UI_SOURCES
    Source/UI/ChoiceButtons.cpp
    Source/UI/Display.cpp
    Source/UI/Knob.cpp
    Source/UI/PadButton.cpp
    Source/UI/PresetButton.cpp)

# The preset file names the plugin by its JucePlugin_* codes, so it builds
# only where juce_add_plugin's definitions are: here, not in FMBoxCheck.
target_sources(FMBox PRIVATE
    Source/Engine/AuPreset.cpp
    ${FMBOX_CORE_SOURCES}
    ${FMBOX_UI_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(FMBox PRIVATE Source)

target_compile_definitions(FMBox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FMBox PRIVATE
    daisysp
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# FMBoxCheck - the engine harness
# ------------------------------------------------------------------------------

juce_add_console_app(FMBoxCheck PRODUCT_NAME "FMBoxCheck")

target_sources(FMBoxCheck PRIVATE
    Verification/FMBoxCheck.cpp
    ${FMBOX_CORE_SOURCES})

target_include_directories(FMBoxCheck PRIVATE Source)

target_compile_definitions(FMBoxCheck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FMBoxCheck PRIVATE
    daisysp
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# EditorShot - renders the panel to a PNG and round-trips the state
# ------------------------------------------------------------------------------

juce_add_console_app(EditorShot PRODUCT_NAME "EditorShot")

target_sources(EditorShot PRIVATE Verification/EditorShot.cpp)

target_compile_definitions(EditorShot PUBLIC
    JUCE_MODAL_LOOPS_PERMITTED=1)

# Linking the plugin's own shared-code library rather than recompiling its
# sources: that library already carries the JucePlugin_* definitions that
# juce_add_plugin generates.
target_link_libraries(EditorShot PRIVATE
    FMBox
    daisysp
    juce::juce_gui_basics
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_include_directories(EditorShot PRIVATE Source)

# Both sides define JUCE_STANDALONE_APPLICATION and the console app's is the
# one that should win.
target_compile_options(EditorShot PRIVATE -Wno-macro-redefined)
