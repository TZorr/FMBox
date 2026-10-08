//
//  FmDrumEngine.h
//  FMBox
//
//  Eight sounds, three send effects, one stereo mix.
//
//      voice (+ click) -> trim -> drive -> level -> pan -> mix
//                                           \-> reverb / delay / mod sends
//      sends -> Kitbox's three effect slots (stereo returns) -> mix
//      mix -> master -> DC blocker
//
//  Each sound is monophonic, as on a drum machine: a new hit replaces the one
//  still ringing. The replaced hit is not cut but faded over 2 ms - every sound
//  keeps two copies of its voice and alternates between them, so the old tail
//  and the new attack overlap for those 2 ms instead of the waveform jumping.
//  A closed hat chokes the open one the same way, over 5 ms.
//
//  A voice that has been below -80 dB for 50 ms stops being computed until its
//  next hit; an idle kit costs only the three effects.
//
//  The sends are mono, post-level and pre-pan, as in Kitbox, and enter their
//  effect as a centred stereo signal.
//

#pragma once

#include "FmVoices.h"
#include "Fx/EffectBlock.h"

#include <array>
#include <atomic>
#include <vector>

class FmDrumEngine
{
public:
    /** Everything about one sound that is not the voice's own four knobs. */
    struct SoundSettings
    {
        VoiceControls controls;
        float drive = 0.0f;      // 0..1
        float click = 0.0f;      // 0..1
        float velocity = 1.0f;   // sensitivity 0..1: 0 plays every hit at full velocity
        float gain = 1.0f;       // level, linear
        float pan = 0.0f;        // -1..1
        std::array<float, FmParams::Fx::numSlots> sends {};   // 0..1
    };

    struct FxSettings
    {
        int type = 0;
        float a = 0.5f, b = 0.5f;
        float gain = 1.0f;       // return level, linear
    };

    FmDrumEngine();

    /** Allocates: the effects build their delay lines here. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Once per block, before render. Gains glide over ~5 ms. */
    void setSound (int sound, const SoundSettings& settings);
    void setFx (int slot, const FxSettings& settings);
    void setMaster (float gain);
    void setTempo (double bpm) { tempo = bpm; }

    /** Audio thread, between render calls - the hit starts on the next sample. */
    void trigger (int sound, float velocity);

    /** Overwrites both channels. Any length; split internally to the block size. */
    void render (float* left, float* right, int numSamples);

    /** For the panel's flashes, from any thread. */
    int getHitCount (int sound) const noexcept       { return slots[(size_t) sound].hits.load(); }
    float getLastVelocity (int sound) const noexcept { return slots[(size_t) sound].velocity.load(); }
    int getLastSound() const noexcept                { return lastSound.load(); }

    /** True while either copy of the sound is still being computed. */
    bool isSounding (int sound) const noexcept;

    /** Per-sound output trims, so the eight models leave at comparable levels
        with every knob at its default: each puts the sound's peak at -6 dBFS
        (FMBoxCheck prints the value), the hats 2 dB lower still because a
        hat's peak undersells how loud it sounds. */
    static constexpr std::array<float, FmParams::numSounds> trims {
        // Kick   Snare  Clap   CH     OH     Tom    Rim    Bell
        0.745f, 0.455f, 2.02f, 0.43f, 0.45f, 0.65f, 0.69f, 0.70f
    };

private:
    /** A short transient on top of a hit: differentiated noise and a 3.5 kHz
        blip, gone in a few milliseconds. */
    struct Click
    {
        float level = 0.0f, coefficient = 0.0f, phase = 0.0f, increment = 0.0f, last = 0.0f;
        FmDsp::Noise noise;

        float next()
        {
            if (level < 1.0e-5f)
                return 0.0f;

            const auto white = noise.next();
            const auto snap = white - last;
            last = white;

            const auto out = (0.25f * snap + 0.3f * std::sin (FmDsp::twoPi * phase)) * level;
            phase += increment;
            phase -= std::floor (phase);
            level *= coefficient;
            return out;
        }
    };

    /** Kitbox's soft clipper, with first-order antiderivative anti-aliasing.
        At zero it is bypassed exactly. */
    struct Drive
    {
        float amount = 0.0f, gain = 1.0f, makeUp = 1.0f, lastIn = 0.0f;

        void set (float newAmount);
        float process (float x);
    };

    struct Copy
    {
        std::unique_ptr<FmVoice> voice;
        Click click;
        bool active = false;
        float fade = 1.0f, fadeStep = 0.0f;
        int age = 0, quiet = 0;
    };

    struct Slot
    {
        std::array<Copy, 2> copies;
        int current = 0;
        SoundSettings settings;
        Drive drive;
        float gainL = 0.0f, gainR = 0.0f, targetL = 0.0f, targetR = 0.0f;
        std::array<float, FmParams::Fx::numSlots> send {}, sendTarget {};
        std::atomic<int> hits { 0 };
        std::atomic<float> velocity { 0.0f };
    };

    void fadeOut (Copy& copy, float seconds);
    void renderChunk (float* left, float* right, int n);

    std::array<Slot, FmParams::numSounds> slots;
    std::array<EffectBlock, FmParams::Fx::numSlots> fxBlocks;
    std::array<FxSettings, FmParams::Fx::numSlots> fxSettings;
    std::array<float, FmParams::Fx::numSlots> fxGain {};
    std::array<std::vector<float>, FmParams::Fx::numSlots> buses;
    std::vector<float> fxLeft, fxRight;
    std::atomic<int> lastSound { -1 };

    float rate = 48000.0f;
    int maxBlock = 512;
    double tempo = 120.0;
    float master = 1.0f, masterTarget = 1.0f, glide = 0.0f;
    float dcCoefficient = 0.999f;
    float dcInL = 0.0f, dcOutL = 0.0f, dcInR = 0.0f, dcOutR = 0.0f;
    float clickCoefficient = 0.99f, clickIncrement = 0.07f;
    int quietLimit = 2400, settleSamples = 480;
};
