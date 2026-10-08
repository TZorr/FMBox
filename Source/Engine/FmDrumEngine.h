//
//  FmDrumEngine.h
//  FMBox
//
//  Eight sounds, one stereo mix.
//
//  Each sound is monophonic, as on a drum machine: a new hit replaces the one
//  still ringing. The replaced hit is not cut but faded over 2 ms - every sound
//  keeps two copies of its voice and alternates between them, so the old tail
//  and the new attack overlap for those 2 ms instead of the waveform jumping.
//  A closed hat chokes the open one the same way, over 5 ms.
//
//  A voice that has been below -80 dB for 50 ms stops being computed until its
//  next hit; an idle kit costs nothing.
//

#pragma once

#include "FmVoices.h"

#include <array>
#include <atomic>

class FmDrumEngine
{
public:
    FmDrumEngine();

    void prepare (double sampleRate);

    /** Once per block, before render: the four knobs, level as a gain and pan
        -1..1. Level and pan glide over ~5 ms. */
    void setControls (int sound, const VoiceControls& controls, float gain, float pan);
    void setMaster (float gain);

    /** Audio thread, between render calls - the hit starts on the next sample. */
    void trigger (int sound, float velocity);

    /** Overwrites both channels. */
    void render (float* left, float* right, int numSamples);

    /** For the panel's flashes, from any thread. */
    int getHitCount (int sound) const noexcept       { return slots[(size_t) sound].hits.load(); }
    float getLastVelocity (int sound) const noexcept { return slots[(size_t) sound].velocity.load(); }

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
    struct Copy
    {
        std::unique_ptr<FmVoice> voice;
        bool active = false;
        float fade = 1.0f, fadeStep = 0.0f;
        int age = 0, quiet = 0;
    };

    struct Slot
    {
        std::array<Copy, 2> copies;
        int current = 0;
        VoiceControls controls;
        float gainL = 0.0f, gainR = 0.0f, targetL = 0.0f, targetR = 0.0f;
        std::atomic<int> hits { 0 };
        std::atomic<float> velocity { 0.0f };
    };

    void fadeOut (Copy& copy, float seconds);

    std::array<Slot, FmParams::numSounds> slots;
    float rate = 48000.0f;
    float master = 1.0f, masterTarget = 1.0f, glide = 0.0f;
    float dcCoefficient = 0.999f;
    float dcInL = 0.0f, dcOutL = 0.0f, dcInR = 0.0f, dcOutR = 0.0f;
    int quietLimit = 2400, settleSamples = 480;
};
