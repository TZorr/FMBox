//
//  PluginProcessor.cpp
//  FMBox
//

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String hz (float value)
    {
        return value >= 1000.0f ? juce::String (value / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (value)) + " Hz";
    }

    juce::String percent (float value)
    {
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";
    }

    juce::String decibels (float value)
    {
        return value <= FmParams::minLevelDb + 0.01f ? juce::String ("-inf")
                                                     : juce::String (value, 1) + " dB";
    }

    juce::String panText (float value)
    {
        const auto amount = juce::roundToInt (std::abs (value) * 100.0f);
        return amount == 0 ? juce::String ("C") : juce::String (amount) + (value < 0.0f ? " L" : " R");
    }

    juce::NormalisableRange<float> tuneRange (const FmParams::SoundSpec& spec)
    {
        // Logarithmic, so an octave is the same turn anywhere on the knob.
        const auto lo = spec.tuneMin, hi = spec.tuneMax;
        return { lo, hi,
                 [] (float start, float end, float x)   { return start * std::pow (end / start, x); },
                 [] (float start, float end, float v)   { return std::log (v / start) / std::log (end / start); },
                 [] (float, float, float v)             { return v; } };
    }
}

FMBoxProcessor::FMBoxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "FMBox", makeParameterLayout())
{
    using namespace FmParams;

    for (int s = 0; s < numSounds; ++s)
    {
        auto& p = soundParams[(size_t) s];
        p.tune  = parameters.getRawParameterValue (soundId (s, Snd::tune));
        p.decay = parameters.getRawParameterValue (soundId (s, Snd::decay));
        p.fm    = parameters.getRawParameterValue (soundId (s, Snd::fm));
        p.tone  = parameters.getRawParameterValue (soundId (s, Snd::tone));
        p.level = parameters.getRawParameterValue (soundId (s, Snd::level));
        p.pan   = parameters.getRawParameterValue (soundId (s, Snd::pan));
        pendingHits[(size_t) s].store (0.0f);
    }

    masterParam = parameters.getRawParameterValue (master);
}

juce::AudioProcessorValueTreeState::ParameterLayout FMBoxProcessor::makeParameterLayout()
{
    using namespace FmParams;
    using Attributes = juce::AudioParameterFloatAttributes;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int s = 0; s < numSounds; ++s)
    {
        const auto& spec = sounds[s];
        const juce::String name (spec.name);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (spec.key, name, " | ");

        const auto id = [s] (const char* suffix) { return juce::ParameterID { soundId (s, suffix), stateVersion }; };

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::tune), name + " Tune", tuneRange (spec), spec.tune,
            Attributes().withStringFromValueFunction ([] (float v, int) { return hz (v); })));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::decay), name + " Decay", juce::NormalisableRange<float> (0.0f, 1.0f), spec.decay,
            Attributes().withStringFromValueFunction ([] (float v, int) { return percent (v); })));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::fm), name + " FM", juce::NormalisableRange<float> (0.0f, 1.0f), spec.fm,
            Attributes().withStringFromValueFunction ([] (float v, int) { return percent (v); })));

        // The Bell's fourth knob is a ratio and says so.
        auto toneText = s == bell ? std::function<juce::String (float, int)> ([] (float v, int) { return juce::String::charToString (0x00d7) + juce::String (bellRatio (v), 2); })
                                  : std::function<juce::String (float, int)> ([] (float v, int) { return percent (v); });

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::tone), name + " " + spec.toneLabel, juce::NormalisableRange<float> (0.0f, 1.0f), spec.tone,
            Attributes().withStringFromValueFunction (toneText)));

        juce::NormalisableRange<float> levelRange (minLevelDb, maxLevelDb);
        levelRange.setSkewForCentre (-12.0f);

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::level), name + " Level", levelRange, 0.0f,
            Attributes().withStringFromValueFunction ([] (float v, int) { return decibels (v); })));

        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            id (Snd::pan), name + " Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
            Attributes().withStringFromValueFunction ([] (float v, int) { return panText (v); })));

        layout.add (std::move (group));
    }

    juce::NormalisableRange<float> masterRange (minLevelDb, maxLevelDb);
    masterRange.setSkewForCentre (-12.0f);

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { master, stateVersion }, "Master", masterRange, 0.0f,
        Attributes().withStringFromValueFunction ([] (float v, int) { return decibels (v); })));

    return layout;
}

//==============================================================================
void FMBoxProcessor::prepareToPlay (double sampleRate, int)
{
    applyParameters();
    engine.prepare (sampleRate);
}

bool FMBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void FMBoxProcessor::applyParameters()
{
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        const auto& p = soundParams[(size_t) s];
        engine.setControls (s, { p.tune->load(), p.decay->load(), p.fm->load(), p.tone->load() },
                            FmParams::levelToGain (p.level->load()), p.pan->load());
    }

    engine.setMaster (FmParams::levelToGain (masterParam->load()));
}

void FMBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
    {
        buffer.clear();
        return;
    }

    applyParameters();

    for (int s = 0; s < FmParams::numSounds; ++s)
        if (const auto velocity = pendingHits[(size_t) s].exchange (0.0f); velocity > 0.0f)
            engine.trigger (s, velocity);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto numSamples = buffer.getNumSamples();

    // Rendered in pieces between MIDI events, so every hit starts on the
    // sample the host put it on.
    int rendered = 0;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (! message.isNoteOn())
            continue;

        const auto position = juce::jlimit (rendered, numSamples, metadata.samplePosition);

        if (position > rendered)
        {
            engine.render (left + rendered, right + rendered, position - rendered);
            rendered = position;
        }

        engine.trigger (FmParams::soundForNote (message.getNoteNumber()), message.getFloatVelocity());
    }

    if (numSamples > rendered)
        engine.render (left + rendered, right + rendered, numSamples - rendered);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

void FMBoxProcessor::playSound (int sound, float velocity)
{
    if (sound >= 0 && sound < FmParams::numSounds)
        pendingHits[(size_t) sound].store (juce::jlimit (0.01f, 1.0f, velocity));
}

//==============================================================================
//  copyState(), never the tree directly: a knob's live value reaches the tree
//  only when the APVTS flushes, and copyState() flushes first.
void FMBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FMBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FMBoxProcessor::createEditor()
{
    return new FMBoxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FMBoxProcessor();
}
