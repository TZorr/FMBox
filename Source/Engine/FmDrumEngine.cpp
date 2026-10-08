//
//  FmDrumEngine.cpp
//  FMBox
//

#include "FmDrumEngine.h"

FmDrumEngine::FmDrumEngine()
{
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        auto& slot = slots[(size_t) s];
        const auto& spec = FmParams::sounds[s];
        slot.controls = { spec.tune, spec.decay, spec.fm, spec.tone };

        for (auto& copy : slot.copies)
            copy.voice = FmVoice::create ((FmParams::Sound) s);
    }

    prepare (48000.0);
}

void FmDrumEngine::prepare (double sampleRate)
{
    rate = (float) sampleRate;
    glide = 1.0f - std::exp (-1.0f / (0.005f * rate));
    dcCoefficient = 1.0f - FmDsp::twoPi * 8.0f / rate;   // 8 Hz
    quietLimit = (int) (0.05f * rate);
    settleSamples = (int) (0.01f * rate);
    dcInL = dcOutL = dcInR = dcOutR = 0.0f;
    master = masterTarget;

    for (auto& slot : slots)
    {
        slot.gainL = slot.targetL;
        slot.gainR = slot.targetR;

        for (auto& copy : slot.copies)
        {
            copy.voice->prepare (rate);
            copy.voice->set (slot.controls);
            copy.active = false;
        }
    }
}

void FmDrumEngine::setControls (int sound, const VoiceControls& controls, float gain, float pan)
{
    auto& slot = slots[(size_t) sound];
    slot.controls = controls;

    for (auto& copy : slot.copies)
        copy.voice->set (controls);

    // Equal power: -3 dB each side at the centre.
    const auto angle = (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    const auto g = gain * trims[(size_t) sound];
    slot.targetL = g * std::cos (angle);
    slot.targetR = g * std::sin (angle);
}

void FmDrumEngine::setMaster (float gain)
{
    masterTarget = gain;
}

void FmDrumEngine::fadeOut (Copy& copy, float seconds)
{
    if (copy.active && copy.fadeStep == 0.0f)
        copy.fadeStep = copy.fade / std::max (1.0f, seconds * rate);
}

void FmDrumEngine::trigger (int sound, float velocity)
{
    if (sound < 0 || sound >= FmParams::numSounds)
        return;

    velocity = juce::jlimit (0.0f, 1.0f, velocity);
    auto& slot = slots[(size_t) sound];

    fadeOut (slot.copies[(size_t) slot.current], 0.002f);
    slot.current ^= 1;

    auto& copy = slot.copies[(size_t) slot.current];
    copy.voice->set (slot.controls);
    copy.voice->trigger (velocity);
    copy.active = true;
    copy.fade = 1.0f;
    copy.fadeStep = 0.0f;
    copy.age = copy.quiet = 0;

    if (sound == FmParams::ch)
        for (auto& open : slots[FmParams::oh].copies)
            fadeOut (open, 0.005f);

    slot.velocity.store (velocity);
    slot.hits.fetch_add (1);
}

bool FmDrumEngine::isSounding (int sound) const noexcept
{
    const auto& copies = slots[(size_t) sound].copies;
    return copies[0].active || copies[1].active;
}

void FmDrumEngine::render (float* left, float* right, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.0f, r = 0.0f;

        for (auto& slot : slots)
        {
            slot.gainL += glide * (slot.targetL - slot.gainL);
            slot.gainR += glide * (slot.targetR - slot.gainR);

            float mono = 0.0f;

            for (auto& copy : slot.copies)
            {
                if (! copy.active)
                    continue;

                const auto x = copy.voice->process() * copy.fade;
                mono += x;

                if (copy.fadeStep > 0.0f)
                {
                    copy.fade -= copy.fadeStep;

                    if (copy.fade <= 0.0f)
                    {
                        copy.active = false;
                        continue;
                    }
                }

                // Asleep after 50 ms below -80 dB - but not in the first
                // 10 ms, where a kick's attack can cross zero slowly.
                copy.quiet = std::abs (x) < 1.0e-4f ? copy.quiet + 1 : 0;

                if (++copy.age > settleSamples && copy.quiet > quietLimit)
                    copy.active = false;
            }

            l += mono * slot.gainL;
            r += mono * slot.gainR;
        }

        master += glide * (masterTarget - master);
        l *= master;
        r *= master;

        // The Plaits kick's transistor VCA leaves an offset under its body;
        // an 8 Hz DC blocker keeps it out of the mix.
        dcOutL = l - dcInL + dcCoefficient * dcOutL;  dcInL = l;
        dcOutR = r - dcInR + dcCoefficient * dcOutR;  dcInR = r;

        left[i]  = dcOutL;
        right[i] = dcOutR;
    }
}
