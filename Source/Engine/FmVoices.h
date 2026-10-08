//
//  FmVoices.h
//  FMBox
//
//  The eight drum models behind one interface.
//
//  Kick, Snare and the two hats are DaisySP's ports of Mutable Instruments
//  Plaits (MIT): SyntheticBassDrum, SyntheticSnareDrum and HiHat. The hats get
//  an FM metallic source in place of the 808's six square waves, faded in by
//  the FM knob. Clap, Tom, Rim and Bell are FMBox's own two-operator voices.
//
//  DaisySP's own Fm2 is not used: it adds the modulator to the carrier's phase
//  increment, so its index is measured against the sample rate and the same
//  setting sounds twice as bright at 96 kHz. The operators here are phase
//  modulation, index in radians, the same at every rate.
//
//  Every voice takes the same four controls - Tune in Hz, Decay, FM and Tone
//  0..1 - and makes of them what fits the drum; ParameterIds.h names the Tone
//  knob per sound.
//

#pragma once

#include "ParameterIds.h"

#include "Drums/hihat.h"
#include "Drums/synthbassdrum.h"
#include "Drums/synthsnaredrum.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

struct VoiceControls
{
    float tune  = 100.0f;   // Hz
    float decay = 0.5f;     // 0..1
    float fm    = 0.3f;     // 0..1
    float tone  = 0.5f;     // 0..1
};

class FmVoice
{
public:
    virtual ~FmVoice() = default;

    virtual void prepare (float sampleRate) = 0;
    virtual void set (const VoiceControls&) = 0;
    virtual void trigger (float velocity) = 0;
    virtual float process() = 0;

    static std::unique_ptr<FmVoice> create (FmParams::Sound sound);
};

//==============================================================================
//  The pieces the own voices are built from.
namespace FmDsp
{
    constexpr float twoPi = 6.283185307179586f;

    /** One-pole exponential decay: multiplied every sample. */
    struct Decay
    {
        float value = 0.0f, coefficient = 0.0f;

        void setTime (float seconds, float sampleRate)
        {
            coefficient = std::exp (-1.0f / (std::max (seconds, 0.0001f) * sampleRate));
        }

        float next() { return value *= coefficient; }
    };

    /** A phase in cycles, wrapped to [0, 1). */
    struct Phase
    {
        float value = 0.0f;

        float advance (float increment)
        {
            value += increment;
            value -= std::floor (value);
            return value;
        }
    };

    /** sin (2 pi carrier + index * sin (2 pi modulator)) */
    inline float pm (float carrierPhase, float modulatorPhase, float index)
    {
        return std::sin (twoPi * carrierPhase + index * std::sin (twoPi * modulatorPhase));
    }

    /** White noise, -1..1, one generator per voice. */
    struct Noise
    {
        uint32_t state = 0x6D2B79F5u;

        float next()
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return static_cast<float> (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
        }
    };

    /** How hard a hit is: level grows with velocity, brightness a little too. */
    inline float velocityGain (float velocity)  { return 0.2f + 0.8f * velocity * velocity; }
    inline float velocityIndex (float velocity) { return 0.6f + 0.4f * velocity; }
}

//==============================================================================
//  The hats' metallic source: three PM pairs at the 808's inharmonic spacing,
//  faded in over the six square waves. HiHat calls Init and Process.
class FmMetalNoise
{
public:
    void Init (float sampleRate);
    float Process (float f0);   // f0 in cycles per sample, as HiHat passes it

    /** 0 = the 808's six squares, 1 = all FM; the index rises with it. */
    void setAmount (float amount) { fm = amount; }

private:
    daisysp::SquareNoise squares;
    FmDsp::Phase carriers[3], modulators[3];
    float fm = 0.0f;
};
