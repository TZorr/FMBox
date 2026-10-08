//
//  PluginEditor.h
//  FMBox
//
//  Eight columns, one per sound: the pad, then Tune / Decay, FM / Tone and
//  Level / Pan. The master level sits in the header. Nothing else.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/Knob.h"
#include "UI/PadButton.h"

class FMBoxEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit FMBoxEditor (FMBoxProcessor&);
    ~FMBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int columnWidth = 118;
    static constexpr int margin = 14;
    static constexpr int headerHeight = 72;
    static constexpr int padHeight = 60;
    static constexpr int knobRowHeight = 76;

private:
    void timerCallback() override;

    struct Column
    {
        std::unique_ptr<PadButton> pad;
        std::array<std::unique_ptr<Knob>, 6> knobs;   // tune, decay, fm, tone, level, pan
    };

    FMBoxProcessor& fmProcessor;
    std::array<Column, FmParams::numSounds> columns;
    Knob masterKnob { "Master" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMBoxEditor)
};
