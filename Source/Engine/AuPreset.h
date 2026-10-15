//
//  AuPreset.h
//  FMBox (from Kitbox)
//
//  Presets on disk: an Audio Unit preset (.aupreset), the file Logic itself
//  writes when it saves an AU setting. A property list naming the plugin
//  (type, subtype, manufacturer), the preset's name, and the plugin's state.
//  Kept in ~/Library/Audio/Presets/<company>/<plugin>/, it is found by Logic's
//  settings menu - and by the plugin's own preset menu (UI/PresetButton.h),
//  which is the only one a VST3 host such as Bitwig has. One library, both
//  formats.
//
//  The state goes under the key JUCE's AU wrapper uses, "jucePluginState", so
//  when Logic loads one of these files the wrapper hands those bytes straight
//  to setStateInformation. What the wrapper checks before that
//  (AUBase::RestoreState): version 0, and subtype and manufacturer matching the
//  plugin. The "data" key of a Logic-written preset holds AU-level parameter
//  values the wrapper discards; it is not written here.
//
//  The identity comes from the JucePlugin_* definitions juce_add_plugin
//  generates, so this file is the same in every plugin of the family.
//

#pragma once

#include <juce_core/juce_core.h>

namespace AuPreset
{
    inline constexpr const char* extension = ".aupreset";

    constexpr int fourCC (const char (&code)[5])
    {
        return (int) (((unsigned) (unsigned char) code[0] << 24) | ((unsigned) (unsigned char) code[1] << 16)
                      | ((unsigned) (unsigned char) code[2] << 8) | (unsigned) (unsigned char) code[3]);
    }

    // AU_MAIN_TYPE in CMakeLists.txt: an instrument is aumu, an effect aufx.
    constexpr int type         = JucePlugin_IsSynth ? fourCC ("aumu") : fourCC ("aufx");
    constexpr int subtype      = JucePlugin_PluginCode;
    constexpr int manufacturer = JucePlugin_ManufacturerCode;

    /** The whole file, ready to write. */
    juce::String write (const juce::MemoryBlock& pluginState, const juce::String& presetName);

    struct Contents
    {
        juce::MemoryBlock pluginState;
        juce::String name;
        juce::String error;   // empty on success

        bool ok() const noexcept { return error.isEmpty(); }
    };

    Contents read (const juce::String& fileText);
}
