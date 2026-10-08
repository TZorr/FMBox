//
//  PluginEditor.h
//  FMBox
//
//  Rackbox Plus's frame around FMBox:
//
//      FMBOX wordmark | display: last hit + send effects | OUTPUT (master)
//      eight sound columns: the pad, then two knobs a row -
//          Tune / Decay, FM / Tone, Drive / Click      the sound
//          Level / Pan, Reverb / Delay, Mod / Velo     the mix
//      REVERB | DELAY | MOD: type buttons, the effect's two knobs, Level
//      footer
//
//  Every knob of every sound is on screen at once: no page, no selection.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/ChoiceButtons.h"
#include "UI/Display.h"
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

private:
    void timerCallback() override;

    /** The effect loaded in a send slot. */
    const EffectCatalog::Info& fxInfo (int slot) const;
    void refreshFx();

    struct Column
    {
        std::unique_ptr<PadButton> pad;
        std::array<std::unique_ptr<Knob>, FmParams::Snd::count> knobs;   // FmParams::Snd::all order
    };

    struct FxSection
    {
        ChoiceButtons types;
        Knob a { "A" }, b { "B" }, level { "Level" };
        int shownType = -1;
    };

    FMBoxProcessor& fmProcessor;
    Display display;
    std::array<Column, FmParams::numSounds> columns;
    std::array<FxSection, FmParams::Fx::numSlots> fx;
    Knob masterKnob { "Master" };

    juce::Rectangle<int> wordmarkArea, outputArea, footerArea;
    std::array<juce::Rectangle<int>, FmParams::numSounds> columnAreas;
    std::array<juce::Rectangle<int>, FmParams::Fx::numSlots> fxAreas;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMBoxEditor)
};
