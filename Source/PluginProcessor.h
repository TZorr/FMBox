//
//  PluginProcessor.h
//  FMBox
//
//  Eight FM drums on the GM drum notes, three send effects, one stereo output.
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
    double getTailLengthSeconds() const override { return 6.0; }   // a hall on the bell

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

    /** The MIDI note that plays the sound. */
    int getSoundNote (int sound) const;

    /** The pad last clicked: what Learn assigns a note to. Not saved. */
    int getSelectedSound() const noexcept       { return selectedSound.load(); }
    void setSelectedSound (int sound) noexcept  { selectedSound.store (juce::jlimit (0, FmParams::numSounds - 1, sound)); }

    /** MIDI Learn: the next note-on the plugin receives becomes this sound's
        note. If another sound had that note, the two swap. The note is taken
        on the audio thread and written to the parameters on the message thread
        by applyLearnedNote(). */
    void startLearn (int sound);
    void cancelLearn()                 { learnSound.store (-1); }
    int getLearnSound() const noexcept { return learnSound.load(); }

    /** Message thread (the editor's timer). Applies a note the audio thread
        has learnt; public so tests need not wait for a timer. */
    void applyLearnedNote();

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout();
    void applyParameters();

    /** A sound's parameters, in FmParams::Snd::all order. */
    using SoundParams = std::array<std::atomic<float>*, FmParams::Snd::count>;

    struct FxParams
    {
        std::atomic<float>* type = nullptr;
        std::atomic<float>* a = nullptr;
        std::atomic<float>* b = nullptr;
        std::atomic<float>* level = nullptr;
    };

    FmDrumEngine engine;
    std::array<SoundParams, FmParams::numSounds> soundParams {};
    std::array<FxParams, FmParams::Fx::numSlots> fxParams {};
    std::array<std::atomic<float>*, FmParams::numSounds> noteParams {};
    std::atomic<float>* masterParam = nullptr;
    std::atomic<int> selectedSound { 0 }, learnSound { -1 }, learnedAssignment { -1 };   // (sound << 8) | note
    std::array<std::atomic<float>, FmParams::numSounds> pendingHits;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FMBoxProcessor)
};
