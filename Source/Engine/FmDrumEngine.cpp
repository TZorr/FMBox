//
//  FmDrumEngine.cpp
//  FMBox
//

#include "FmDrumEngine.h"

namespace
{
    /** Kitbox's soft clipper: x (27 + x^2) / (27 + 9 x^2), flat beyond +-3. */
    inline float softClip (float x)
    {
        x = juce::jlimit (-3.0f, 3.0f, x);
        const auto x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    /** Its antiderivative: x^2 / 18 + 4/3 ln (x^2 + 3), continued with slope
        1 beyond +-3, where the clipper is flat at +-1. */
    inline float softClipIntegral (float x)
    {
        const auto ax = std::abs (x);
        static const float atThree = 9.0f / 18.0f + (4.0f / 3.0f) * std::log (12.0f);

        if (ax >= 3.0f)
            return atThree + (ax - 3.0f);

        return x * x / 18.0f + (4.0f / 3.0f) * std::log (x * x + 3.0f);
    }
}

//==============================================================================
void FmDrumEngine::Drive::set (float newAmount)
{
    amount = newAmount;
    gain   = std::pow (10.0f, amount * 24.0f / 20.0f);   // up to +24 dB into the clipper
    makeUp = 1.0f / std::sqrt (gain);
}

float FmDrumEngine::Drive::process (float x)
{
    const auto in = x * gain;
    const auto previous = lastIn;
    lastIn = in;

    if (amount <= 0.0f)
        return x;

    const auto delta = in - previous;
    const auto shaped = std::abs (delta) > 1.0e-4f
                          ? (softClipIntegral (in) - softClipIntegral (previous)) / delta
                          : softClip (0.5f * (in + previous));

    // Faded in over the first 10 % so the knob leaves zero without a jump.
    const auto blend = amount >= 0.1f ? 1.0f : amount * 10.0f;
    return x + blend * (shaped * makeUp - x);
}

//==============================================================================
FmDrumEngine::FmDrumEngine()
{
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        auto& slot = slots[(size_t) s];
        const auto& spec = FmParams::sounds[s];
        slot.settings.controls = { spec.tune, spec.decay, spec.fm, spec.tone };

        for (auto& copy : slot.copies)
            copy.voice = FmVoice::create ((FmParams::Sound) s);
    }

    for (int f = 0; f < FmParams::Fx::numSlots; ++f)
        fxSettings[(size_t) f] = { FmParams::Fx::defaultTypes[f], FmParams::Fx::defaultA[f], FmParams::Fx::defaultB[f], 1.0f };

    prepare (48000.0, 512);
}

void FmDrumEngine::prepare (double sampleRate, int maxBlockSize)
{
    rate = (float) sampleRate;
    maxBlock = juce::jmax (1, maxBlockSize);
    glide = 1.0f - std::exp (-1.0f / (0.005f * rate));
    dcCoefficient = 1.0f - FmDsp::twoPi * 8.0f / rate;   // 8 Hz
    quietLimit = (int) (0.05f * rate);
    settleSamples = (int) (0.01f * rate);
    clickCoefficient = std::exp (-1.0f / (0.0012f * rate));
    clickIncrement = 3500.0f / rate;
    dcInL = dcOutL = dcInR = dcOutR = 0.0f;
    master = masterTarget;

    for (auto& slot : slots)
    {
        slot.gainL = slot.targetL;
        slot.gainR = slot.targetR;
        slot.send = slot.sendTarget;
        slot.drive.lastIn = 0.0f;

        for (auto& copy : slot.copies)
        {
            copy.voice->prepare (rate);
            copy.voice->set (slot.settings.controls);
            copy.active = false;
            copy.click.level = 0.0f;
        }
    }

    for (size_t f = 0; f < fxBlocks.size(); ++f)
    {
        fxBlocks[f].prepare (sampleRate, maxBlock);
        fxGain[f] = fxSettings[f].gain;
        buses[f].assign ((size_t) maxBlock, 0.0f);
    }

    fxLeft.assign ((size_t) maxBlock, 0.0f);
    fxRight.assign ((size_t) maxBlock, 0.0f);
}

void FmDrumEngine::setSound (int sound, const SoundSettings& settings)
{
    auto& slot = slots[(size_t) sound];
    slot.settings = settings;

    for (auto& copy : slot.copies)
        copy.voice->set (settings.controls);

    if (! juce::exactlyEqual (slot.drive.amount, settings.drive))
        slot.drive.set (settings.drive);

    // Equal power: -3 dB each side at the centre.
    const auto angle = (juce::jlimit (-1.0f, 1.0f, settings.pan) + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    slot.targetL = settings.gain * std::cos (angle);
    slot.targetR = settings.gain * std::sin (angle);

    // Post-level: a sound turned down sends less.
    for (size_t f = 0; f < slot.sendTarget.size(); ++f)
        slot.sendTarget[f] = settings.gain * settings.sends[f];
}

void FmDrumEngine::setFx (int slot, const FxSettings& settings)
{
    fxSettings[(size_t) slot] = settings;
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

    // Sensitivity 1 plays the velocity as it came, 0 plays every hit at full.
    const auto played = 1.0f - slot.settings.velocity * (1.0f - velocity);

    fadeOut (slot.copies[(size_t) slot.current], 0.002f);
    slot.current ^= 1;

    auto& copy = slot.copies[(size_t) slot.current];
    copy.voice->set (slot.settings.controls);
    copy.voice->trigger (played);
    copy.active = true;
    copy.fade = 1.0f;
    copy.fadeStep = 0.0f;
    copy.age = copy.quiet = 0;

    copy.click.level = slot.settings.click * played;
    copy.click.coefficient = clickCoefficient;
    copy.click.increment = clickIncrement;
    copy.click.phase = 0.0f;

    if (sound == FmParams::ch)
        for (auto& open : slots[FmParams::oh].copies)
            fadeOut (open, 0.005f);

    slot.velocity.store (velocity);
    slot.hits.fetch_add (1);
    lastSound.store (sound);
}

bool FmDrumEngine::isSounding (int sound) const noexcept
{
    const auto& copies = slots[(size_t) sound].copies;
    return copies[0].active || copies[1].active;
}

void FmDrumEngine::render (float* left, float* right, int numSamples)
{
    while (numSamples > 0)
    {
        const auto n = juce::jmin (numSamples, maxBlock);
        renderChunk (left, right, n);
        left += n;
        right += n;
        numSamples -= n;
    }
}

void FmDrumEngine::renderChunk (float* left, float* right, int n)
{
    std::fill (left, left + n, 0.0f);
    std::fill (right, right + n, 0.0f);

    for (auto& bus : buses)
        std::fill (bus.begin(), bus.begin() + n, 0.0f);

    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        auto& slot = slots[(size_t) s];
        const auto trim = trims[(size_t) s];
        const auto silent = ! slot.copies[0].active && ! slot.copies[1].active;

        // A silent sound only keeps its gains gliding; its drive forgets the
        // last sample so the next hit's first difference starts from zero.
        if (silent)
            slot.drive.lastIn = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            slot.gainL += glide * (slot.targetL - slot.gainL);
            slot.gainR += glide * (slot.targetR - slot.gainR);

            for (size_t f = 0; f < slot.send.size(); ++f)
                slot.send[f] += glide * (slot.sendTarget[f] - slot.send[f]);

            if (silent)
                continue;

            float mono = 0.0f;

            for (auto& copy : slot.copies)
            {
                if (! copy.active)
                    continue;

                const auto x = (copy.voice->process() * trim + 0.8f * copy.click.next()) * copy.fade;
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

            mono = slot.drive.process (mono);

            const auto l = mono * slot.gainL;
            const auto r = mono * slot.gainR;

            left[i]  += l;
            right[i] += r;

            for (size_t f = 0; f < buses.size(); ++f)
                buses[f][(size_t) i] += mono * slot.send[f];
        }
    }

    // The send effects. A send enters as a centred stereo signal - -3 dB per
    // side, like a centred sound - so an echo comes back at the sound's level.
    constexpr float centre = 0.70710678f;

    for (int f = 0; f < FmParams::Fx::numSlots; ++f)
    {
        const auto& p = fxSettings[(size_t) f];

        EffectBlock::Settings settings;
        settings.category = FmParams::Fx::categories[f];
        settings.subtype  = EffectCatalog::clampSubtype (settings.category, p.type);
        settings.a        = p.a;
        settings.b        = p.b;
        settings.mix      = 1.0f;
        settings.sync     = f == FmParams::Fx::delay;   // the delay always follows the tempo, as in Kitbox
        settings.bpm      = tempo;

        const auto& bus = buses[(size_t) f];

        for (int i = 0; i < n; ++i)
            fxLeft[(size_t) i] = fxRight[(size_t) i] = bus[(size_t) i] * centre;

        fxBlocks[(size_t) f].process (fxLeft.data(), fxRight.data(), n, settings);

        // The return level ramps across the chunk.
        const auto start = fxGain[(size_t) f];
        const auto step  = (p.gain - start) / (float) n;
        auto gain = start;

        for (int i = 0; i < n; ++i)
        {
            gain += step;
            left[i]  += gain * fxLeft[(size_t) i];
            right[i] += gain * fxRight[(size_t) i];
        }

        fxGain[(size_t) f] = p.gain;
    }

    for (int i = 0; i < n; ++i)
    {
        master += glide * (masterTarget - master);
        const auto l = left[i] * master;
        const auto r = right[i] * master;

        // The Plaits kick's transistor VCA leaves an offset under its body;
        // an 8 Hz DC blocker keeps it out of the mix.
        dcOutL = l - dcInL + dcCoefficient * dcOutL;  dcInL = l;
        dcOutR = r - dcInR + dcCoefficient * dcOutR;  dcInR = r;

        left[i]  = dcOutL;
        right[i] = dcOutR;
    }
}
