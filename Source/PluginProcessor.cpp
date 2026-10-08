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
        for (int k = 0; k < Snd::count; ++k)
            soundParams[(size_t) s][(size_t) k] = parameters.getRawParameterValue (soundId (s, Snd::all[k]));

        noteParams[(size_t) s] = parameters.getRawParameterValue (soundId (s, Snd::note));

        pendingHits[(size_t) s].store (0.0f);
    }

    for (int f = 0; f < Fx::numSlots; ++f)
    {
        const auto& ids = Fx::ids[f];
        fxParams[(size_t) f] = { parameters.getRawParameterValue (ids.type), parameters.getRawParameterValue (ids.a),
                                 parameters.getRawParameterValue (ids.b), parameters.getRawParameterValue (ids.level) };
    }

    masterParam = parameters.getRawParameterValue (master);
}

juce::AudioProcessorValueTreeState::ParameterLayout FMBoxProcessor::makeParameterLayout()
{
    using namespace FmParams;
    using Attributes = juce::AudioParameterFloatAttributes;
    using TextFunction = std::function<juce::String (float, int)>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const TextFunction percentText = [] (float v, int) { return percent (v); };
    const TextFunction dbText      = [] (float v, int) { return decibels (v); };

    juce::NormalisableRange<float> levelRange (minLevelDb, maxLevelDb);
    levelRange.setSkewForCentre (-12.0f);

    const juce::NormalisableRange<float> unit (0.0f, 1.0f);

    for (int s = 0; s < numSounds; ++s)
    {
        const auto& spec = sounds[s];
        const juce::String name (spec.name);
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> (spec.key, name, " | ");

        // Version hint 1 for the parameters of 0.1.0, 2 for those added in
        // 0.2.0: hosts use the hint to keep the order of older ones stable.
        const auto add = [&] (const char* suffix, const juce::String& label, juce::NormalisableRange<float> range,
                              float value, const TextFunction& text, int version)
        {
            group->addChild (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { soundId (s, suffix), version }, name + " " + label, range, value,
                Attributes().withStringFromValueFunction (text)));
        };

        // The Bell's fourth knob is a ratio and says so.
        const auto toneText = s == bell ? TextFunction ([] (float v, int) { return juce::String::charToString (0x00d7) + juce::String (bellRatio (v), 2); })
                                        : percentText;

        add (Snd::tune,     "Tune",        tuneRange (spec), spec.tune, [] (float v, int) { return hz (v); }, 1);
        add (Snd::decay,    "Decay",       unit, spec.decay, percentText, 1);
        add (Snd::fm,       "FM",          unit, spec.fm,    percentText, 1);
        add (Snd::tone,     spec.toneLabel, unit, spec.tone, toneText, 1);
        add (Snd::level,    "Level",       levelRange, 0.0f, dbText, 1);
        add (Snd::pan,      "Pan",         { -1.0f, 1.0f }, 0.0f, [] (float v, int) { return panText (v); }, 1);
        add (Snd::drive,    "Drive",       unit, 0.0f, percentText, 2);
        add (Snd::click,    "Click",       unit, 0.0f, percentText, 2);
        add (Snd::velocity, "Velocity",    unit, 1.0f, percentText, 2);
        add (Snd::reverb,   "Reverb Send", unit, spec.reverb, percentText, 2);
        add (Snd::delay,    "Delay Send",  unit, 0.0f, percentText, 2);
        add (Snd::mod,      "Mod Send",    unit, 0.0f, percentText, 2);

        group->addChild (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { soundId (s, Snd::note), 3 }, name + " Note", 0, 127, spec.note,
            juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v) + " " + juce::String (v); })));

        layout.add (std::move (group));
    }

    // The send effects: Type, the effect's two knobs, the return level. A and
    // B mean whatever the loaded effect calls them (EffectCatalog), so here
    // they are plain 0..1 - the panel prints them in the effect's own units.
    auto fxGroup = std::make_unique<juce::AudioProcessorParameterGroup> ("fx", "Effects", " | ");

    for (int f = 0; f < Fx::numSlots; ++f)
    {
        const auto& ids = Fx::ids[f];
        const juce::String title (Fx::titles[f]);
        const auto category = Fx::categories[f];

        juce::StringArray types;
        for (int t = 0; t < EffectCatalog::numSubtypes (category); ++t)
            types.add (EffectCatalog::info (category, t).name);

        fxGroup->addChild (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ids.type, 2 }, title + " Type", types, Fx::defaultTypes[f]));

        fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ids.a, 2 }, title + " A", unit, Fx::defaultA[f], Attributes().withStringFromValueFunction (percentText)));
        fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ids.b, 2 }, title + " B", unit, Fx::defaultB[f], Attributes().withStringFromValueFunction (percentText)));
        fxGroup->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ids.level, 2 }, title + " Level", levelRange, 0.0f, Attributes().withStringFromValueFunction (dbText)));
    }

    layout.add (std::move (fxGroup));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { master, stateVersion }, "Master", levelRange, 0.0f,
        Attributes().withStringFromValueFunction (dbText)));

    return layout;
}

//==============================================================================
void FMBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    applyParameters();
    engine.prepare (sampleRate, juce::jmax (1, samplesPerBlock));
}

bool FMBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void FMBoxProcessor::applyParameters()
{
    using namespace FmParams;

    for (int s = 0; s < numSounds; ++s)
    {
        const auto& p = soundParams[(size_t) s];
        const auto get = [&p] (const char* suffix)
        {
            for (int k = 0; k < Snd::count; ++k)
                if (Snd::all[k] == suffix)
                    return p[(size_t) k]->load();
            return 0.0f;
        };

        FmDrumEngine::SoundSettings settings;
        settings.controls = { get (Snd::tune), get (Snd::decay), get (Snd::fm), get (Snd::tone) };
        settings.drive    = get (Snd::drive);
        settings.click    = get (Snd::click);
        settings.velocity = get (Snd::velocity);
        settings.gain     = levelToGain (get (Snd::level));
        settings.pan      = get (Snd::pan);
        settings.sends    = { get (Snd::reverb), get (Snd::delay), get (Snd::mod) };
        engine.setSound (s, settings);
    }

    for (int f = 0; f < Fx::numSlots; ++f)
    {
        const auto& p = fxParams[(size_t) f];
        engine.setFx (f, { (int) std::lround (p.type->load()), p.a->load(), p.b->load(), levelToGain (p.level->load()) });
    }

    engine.setMaster (levelToGain (masterParam->load()));
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

    if (auto* playHead = getPlayHead())
        if (auto position = playHead->getPosition())
            if (auto bpm = position->getBpm())
                engine.setTempo (*bpm);

    for (int s = 0; s < FmParams::numSounds; ++s)
        if (const auto velocity = pendingHits[(size_t) s].exchange (0.0f); velocity > 0.0f)
            engine.trigger (s, velocity);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const auto numSamples = buffer.getNumSamples();

    // Rendered in pieces between MIDI events, so every hit starts on the
    // sample the host put it on.
    int rendered = 0;

    std::array<int, FmParams::numSounds> notes {};
    for (int s = 0; s < FmParams::numSounds; ++s)
        notes[(size_t) s] = getSoundNote (s);

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

        const auto note = message.getNoteNumber();

        // Learn takes the note for the sound that asked, and the note still
        // plays whatever it played - hearing the key is the confirmation.
        if (const auto learning = learnSound.exchange (-1); learning >= 0)
            learnedAssignment.store ((learning << 8) | note);

        for (int s = 0; s < FmParams::numSounds; ++s)
            if (notes[(size_t) s] == note)
                engine.trigger (s, message.getFloatVelocity());
    }

    if (numSamples > rendered)
        engine.render (left + rendered, right + rendered, numSamples - rendered);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);
}

int FMBoxProcessor::getSoundNote (int sound) const
{
    return (int) std::lround (noteParams[(size_t) sound]->load());
}

void FMBoxProcessor::startLearn (int sound)
{
    learnedAssignment.store (-1);
    learnSound.store (juce::jlimit (0, FmParams::numSounds - 1, sound));
}

void FMBoxProcessor::applyLearnedNote()
{
    const auto learned = learnedAssignment.exchange (-1);

    if (learned < 0)
        return;

    const auto sound = learned >> 8, note = learned & 0x7f;
    const auto previous = getSoundNote (sound);

    const auto setNote = [this] (int s, int n)
    {
        auto* parameter = parameters.getParameter (FmParams::soundId (s, FmParams::Snd::note));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) n));
        parameter->endChangeGesture();
    };

    // Whoever had the note takes this sound's old one: every sound stays
    // playable and no note plays two.
    for (int s = 0; s < FmParams::numSounds; ++s)
        if (s != sound && getSoundNote (s) == note)
            setNote (s, previous);

    setNote (sound, note);
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
