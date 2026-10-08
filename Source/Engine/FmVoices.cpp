//
//  FmVoices.cpp
//  FMBox
//

#include "FmVoices.h"

#include "Filters/svf.h"

#include <algorithm>
#include <cmath>

using namespace FmDsp;

namespace
{
    /** A phase increment, kept below Nyquist. */
    inline float increment (float hz, float sampleRate)
    {
        return std::min (hz / sampleRate, 0.49f);
    }

    //==============================================================================
    //  DaisySP / Plaits
    class Kick final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override
        {
            drum.Init (sampleRate);
            drum.SetDirtiness (0.2f);
            drum.SetFmEnvelopeDecay (0.25f);
        }

        void set (const VoiceControls& c) override
        {
            drum.SetFreq (c.tune);
            drum.SetDecay (c.decay);
            drum.SetFmEnvelopeAmount (c.fm);   // the pitch sweep of the 909-ish body
            drum.SetTone (c.tone);
        }

        void trigger (float velocity) override
        {
            drum.SetAccent (velocity);
            drum.Trig();
        }

        float process() override { return drum.Process(); }

    private:
        daisysp::SyntheticBassDrum drum;
    };

    class Snare final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override { drum.Init (sampleRate); }

        void set (const VoiceControls& c) override
        {
            drum.SetFreq (c.tune);
            drum.SetDecay (c.decay);
            drum.SetFmAmount (c.fm);
            drum.SetSnappy (c.tone);
        }

        void trigger (float velocity) override
        {
            drum.SetAccent (velocity);
            drum.Trig();
        }

        float process() override { return drum.Process(); }

    private:
        daisysp::SyntheticSnareDrum drum;
    };

    class Hat final : public FmVoice
    {
    public:
        explicit Hat (bool isOpen) : open (isOpen) {}

        void prepare (float sampleRate) override
        {
            hat.Init (sampleRate);
            hat.SetNoisiness (0.2f);
        }

        void set (const VoiceControls& c) override
        {
            hat.SetFreq (c.tune);
            hat.SetTone (c.tone);
            hat.GetMetallicNoise().setAmount (c.fm);

            // HiHat's own decay runs 0 (a tick) to 1 (about a second) but its
            // setter maps x to 1.7x - 1.2; undone here so each hat gets its
            // half of the range: closed 0 .. 0.55, open 0.4 .. 1.
            const auto d = open ? 0.4f + 0.6f * c.decay : 0.55f * c.decay;
            hat.SetDecay ((d + 1.2f) / 1.7f);
        }

        void trigger (float velocity) override
        {
            hat.SetAccent (velocity);
            hat.Trig();
        }

        float process() override { return hat.Process(); }

    private:
        const bool open;
        daisysp::HiHat<FmMetalNoise, daisysp::LinearVCA, true> hat;
    };

    //==============================================================================
    //  FMBox's own

    /** Four noise bursts a few milliseconds apart, then a tail, through a
        band-pass at Tune. FM fades the white noise over to a high-index PM
        pair, which is noise with a pitch in it. Tone spreads the bursts. */
    class Clap final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override
        {
            rate = sampleRate;
            filter.Init (sampleRate);
            filter.SetRes (0.45f);
            burst.setTime (0.0025f, rate);
        }

        void set (const VoiceControls& c) override
        {
            frequency = c.tune;
            filter.SetFreq (std::min (c.tune, rate * 0.3f));
            fm = c.fm;
            spacing = std::max (1, (int) ((0.004f + 0.012f * c.tone) * rate));
            tail.setTime (0.03f + 0.5f * c.decay * c.decay, rate);
        }

        void trigger (float velocity) override
        {
            gain = velocityGain (velocity);
            burstsLeft = 4;
            countdown = 1;
            tail.value = 0.0f;
        }

        float process() override
        {
            if (burstsLeft > 0 && --countdown <= 0)
            {
                burst.value = 1.0f;
                countdown = spacing;

                if (--burstsLeft == 0)
                    tail.value = 0.6f;
            }

            const auto envelope = std::max (burst.next(), tail.next());

            const auto white = noise.next();
            const auto metal = pm (carrier.value, modulator.value, 9.0f);
            carrier.advance (increment (frequency * 0.5f, rate));
            modulator.advance (increment (frequency * 1.37f, rate));

            filter.Process (white + fm * (metal - white));
            return filter.Band() * envelope * gain;
        }

    private:
        float rate = 48000.0f, frequency = 1000.0f, fm = 0.0f, gain = 0.0f;
        int spacing = 1, countdown = 0, burstsLeft = 0;
        Decay burst, tail;
        Phase carrier, modulator;
        Noise noise;
        daisysp::Svf filter;
    };

    /** A PM pair at a membrane's first overtone ratio (1.59), its pitch
        falling from up to 2.5x Tune (Bend) and its index fading faster than
        its level, so the hit starts bright and rings round. */
    class Tom final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override
        {
            rate = sampleRate;
            pitch.setTime (0.03f, rate);
        }

        void set (const VoiceControls& c) override
        {
            frequency = c.tune;
            bend = 1.5f * c.tone;
            index = 5.0f * c.fm;

            const auto time = 0.04f + 1.2f * c.decay * c.decay;
            level.setTime (time, rate);
            brightness.setTime (0.35f * time, rate);
        }

        void trigger (float velocity) override
        {
            carrier.value = modulator.value = 0.0f;
            level.value = velocityGain (velocity);
            brightness.value = velocityIndex (velocity);
            pitch.value = 1.0f;
        }

        float process() override
        {
            const auto f = frequency * (1.0f + bend * pitch.next());
            const auto out = pm (carrier.value, modulator.value, index * brightness.next());

            carrier.advance (increment (f, rate));
            modulator.advance (increment (f * 1.59f, rate));

            return out * level.next();
        }

    private:
        float rate = 48000.0f, frequency = 100.0f, bend = 0.0f, index = 0.0f;
        Phase carrier, modulator;
        Decay level, brightness, pitch;
    };

    /** A short, inharmonic PM pair (ratio 2.27) over a body a quarter of its
        pitch - the 808 rim's two tones - and a click of differentiated noise. */
    class Rim final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override
        {
            rate = sampleRate;
            brightness.setTime (0.004f, rate);
            click.setTime (0.0007f, rate);
        }

        void set (const VoiceControls& c) override
        {
            frequency = c.tune;
            index = 10.0f * c.fm;
            clickAmount = c.tone;
            level.setTime (0.004f + 0.08f * c.decay * c.decay, rate);
        }

        void trigger (float velocity) override
        {
            carrier.value = modulator.value = body.value = 0.0f;
            level.value = velocityGain (velocity);
            brightness.value = velocityIndex (velocity);
            click.value = level.value;
        }

        float process() override
        {
            const auto tone = 0.7f * pm (carrier.value, modulator.value, index * brightness.next())
                            + 0.4f * std::sin (twoPi * body.value);

            carrier.advance (increment (frequency, rate));
            modulator.advance (increment (frequency * 2.27f, rate));
            body.advance (increment (frequency * 0.27f, rate));

            const auto white = noise.next();
            const auto snap = (white - lastNoise) * 0.5f;
            lastNoise = white;

            return tone * level.next() + snap * click.next() * clickAmount;
        }

    private:
        float rate = 48000.0f, frequency = 1000.0f, index = 0.0f, clickAmount = 0.0f, lastNoise = 0.0f;
        Phase carrier, modulator, body;
        Decay level, brightness, click;
        Noise noise;
    };

    /** Two PM pairs a ratio of 1.48 apart - a cowbell's two tones at low FM,
        a bell's cluster at high FM. Tone sets the modulator ratio, 1 .. 4. */
    class Bell final : public FmVoice
    {
    public:
        void prepare (float sampleRate) override { rate = sampleRate; }

        void set (const VoiceControls& c) override
        {
            frequency = c.tune;
            ratio = FmParams::bellRatio (c.tone);
            index = 8.0f * c.fm;

            const auto time = 0.05f + 2.4f * c.decay * c.decay;
            level.setTime (time, rate);
            brightness.setTime (0.6f * time, rate);
        }

        void trigger (float velocity) override
        {
            for (auto* p : { &carriers[0], &carriers[1], &modulators[0], &modulators[1] })
                p->value = 0.0f;

            level.value = velocityGain (velocity);
            brightness.value = velocityIndex (velocity);
        }

        float process() override
        {
            const auto i = index * brightness.next();
            const auto out = 0.6f * pm (carriers[0].value, modulators[0].value, i)
                           + 0.4f * pm (carriers[1].value, modulators[1].value, 0.8f * i);

            const float f[2] = { frequency, frequency * 1.48f };

            for (int k = 0; k < 2; ++k)
            {
                carriers[k].advance (increment (f[k], rate));
                modulators[k].advance (increment (f[k] * ratio, rate));
            }

            return out * level.next();
        }

    private:
        float rate = 48000.0f, frequency = 500.0f, ratio = 1.41f, index = 0.0f;
        Phase carriers[2], modulators[2];
        Decay level, brightness;
    };
}

//==============================================================================
std::unique_ptr<FmVoice> FmVoice::create (FmParams::Sound sound)
{
    switch (sound)
    {
        case FmParams::bd:   return std::make_unique<Kick>();
        case FmParams::sd:   return std::make_unique<Snare>();
        case FmParams::cp:   return std::make_unique<Clap>();
        case FmParams::ch:   return std::make_unique<Hat> (false);
        case FmParams::oh:   return std::make_unique<Hat> (true);
        case FmParams::tom:  return std::make_unique<Tom>();
        case FmParams::rim:  return std::make_unique<Rim>();
        case FmParams::bell: return std::make_unique<Bell>();
        case FmParams::numSounds: break;
    }

    return nullptr;
}

//==============================================================================
void FmMetalNoise::Init (float sampleRate)
{
    squares.Init (sampleRate);
}

float FmMetalNoise::Process (float f0)
{
    const auto square = squares.Process (f0);

    if (fm <= 0.0f)
        return square;

    // Carriers at three of the 808's six ratios, each with an irrational
    // modulator so the sidebands never line up into a pitch.
    static constexpr float carrierRatios[3]   = { 1.0f, 1.466f, 1.932f };
    static constexpr float modulatorRatios[3] = { 1.414f, 2.236f, 1.732f };

    const auto index = 1.5f + 6.0f * fm;
    float metal = 0.0f;

    for (int k = 0; k < 3; ++k)
    {
        const auto c = std::min (f0 * carrierRatios[k], 0.49f);
        metal += pm (carriers[k].value, modulators[k].value, index);
        carriers[k].advance (c);
        modulators[k].advance (std::min (c * modulatorRatios[k], 0.49f));
    }

    metal *= 1.0f / 3.0f;
    return square + fm * (metal - square);
}
