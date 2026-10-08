//
//  PluginProcessor.h
//  FMBox
//
//  Eight FM drums on the GM drum notes, one stereo output.
//
//  The state is the parameter tree and nothing else: no samples, no files, no
//  kit format. A session or an .aupreset restores the kit completely.
//

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/FmDrumEngine.h"
#include "ParameterIds.h"

class FMBoxProcessor : public juce::AudioProcessor
{
public:
    FMBoxProcessor();
    ~FMBoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Plays one sound from the message thread (a click on its pad). The hit
        lands at the start of the next block. */
    void playSound (int sound, float velocity);

    const FmDrumEngine& getEngine() const noexcept { return engine; }

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    void applyParameters();

    struct SoundParams
    {
        std::atomic<float>* tune = nullptr;
        std::atomic<float>* decay = nullptr;
        std::atomic<float>* fm = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* level = nullptr;
        std::atomic<float>* pan = nullptr;
    };

    FmDrumEngine engine;
    std::array<SoundParams, FmParams::numSounds> soundParams;
    std::atomic<float>* masterParam = nullptr;
    std::array<std::atomic<float>, FmParams::numSounds> pendingHits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMBoxProcessor)
};
