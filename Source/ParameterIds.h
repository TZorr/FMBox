//
//  ParameterIds.h
//  FMBox
//
//  Every parameter the host can see, and the eight sounds they belong to.
//
//  A sound's parameters are the same twelve suffixes eight times over, with the
//  sound's short name in front: "sd_decay". The ids are the identity a saved
//  session is restored by, so once a session exists none of these strings may
//  change - rename the display name instead.
//
//  Every knob is read once per block, so turning Decay or Tune while a hit is
//  still ringing changes the hit, as on an analogue drum machine.
//

#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

namespace FmParams
{
    constexpr int stateVersion = 1;

    enum Sound { bd, sd, cp, ch, oh, tom, rim, bell, numSounds };

    /** What sets one sound apart from the next: its name, its note, the range
        of its Tune knob and the defaults the kit opens with. Tune is in Hz;
        Decay, FM and Tone are 0..1 (shown as percent). */
    struct SoundSpec
    {
        const char* key;        // id prefix - never changes
        const char* name;       // on the pad
        const char* toneLabel;  // what the fourth knob does on this sound
        int note;               // GM drum map
        float tuneMin, tuneMax, tune;
        float decay, fm, tone;
        float reverb;           // send, 0..1 - a little room on snare and clap
    };

    inline constexpr SoundSpec sounds[numSounds] = {
        { "bd",   "Kick",       "Tone",   36,   30.0f,  150.0f,   50.0f, 0.55f, 0.55f, 0.45f,  0.0f  },
        { "sd",   "Snare",      "Snappy", 38,  100.0f,  450.0f,  200.0f, 0.35f, 0.25f, 0.60f,  0.15f },
        { "cp",   "Clap",       "Spread", 39,  500.0f, 3000.0f, 1150.0f, 0.35f, 0.20f, 0.45f,  0.20f },
        { "ch",   "Closed Hat", "Tone",   42,  100.0f, 1200.0f,  300.0f, 0.40f, 0.30f, 0.80f,  0.0f  },
        { "oh",   "Open Hat",   "Tone",   46,  100.0f, 1200.0f,  300.0f, 0.55f, 0.30f, 0.75f,  0.0f  },
        { "tom",  "Tom",        "Bend",   45,   50.0f,  400.0f,  120.0f, 0.40f, 0.30f, 0.40f,  0.0f  },
        { "rim",  "Rim",        "Click",  37,  300.0f, 2500.0f, 1100.0f, 0.20f, 0.45f, 0.50f,  0.0f  },
        { "bell", "Bell",       "Ratio",  56,  150.0f, 2000.0f,  560.0f, 0.22f, 0.35f, 0.137f, 0.0f  },
    };

    /** The Bell's Tone knob is the modulator ratio, 1.0 .. 4.0. */
    inline float bellRatio (float tone) { return 1.0f + 3.0f * tone; }

    /** "C1" for 36 - Logic's convention, middle C = C3. */
    inline juce::String noteName (int note)
    {
        static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[note % 12]) + juce::String (note / 12 - 2);
    }

    //==============================================================================
    //  Per sound
    namespace Snd
    {
        inline constexpr const char* tune     = "tune";
        inline constexpr const char* decay    = "decay";
        inline constexpr const char* fm       = "fm";
        inline constexpr const char* tone     = "tone";
        inline constexpr const char* level    = "level";
        inline constexpr const char* pan      = "pan";

        //  Added in 0.2.0 (version hint 2).
        inline constexpr const char* drive    = "drive";      // saturation after the voice, 0..1
        inline constexpr const char* click    = "click";      // a transient on top of the hit, 0..1
        inline constexpr const char* velocity = "velocity";   // how much velocity matters, 0..1
        inline constexpr const char* reverb   = "rev";        // sends, 0..1, post-level, pre-pan
        inline constexpr const char* delay    = "dly";
        inline constexpr const char* mod      = "mod";

        /** In the order the panel shows them, two per row. */
        inline constexpr const char* all[] = { tune, decay, fm, tone, drive, click,
                                               level, pan, reverb, delay, mod, velocity };

        constexpr int count = 12;

        //  Added in 0.3.0 (version hint 3): the MIDI note that plays the
        //  sound, GM by default, changed by Learn. Not a knob, so not in all[].
        inline constexpr const char* note = "note";
    }

    /** "sd_decay" for the snare. */
    inline juce::String soundId (int sound, const char* suffix)
    {
        return juce::String (sounds[sound].key) + "_" + suffix;
    }

    //==============================================================================
    //  Global
    inline constexpr const char* master = "master";

    constexpr float minLevelDb = -60.0f;   // the bottom of a level knob is silence
    constexpr float maxLevelDb = 6.0f;

    inline float levelToGain (float db)
    {
        return db <= minLevelDb + 0.01f ? 0.0f : std::pow (10.0f, db * 0.05f);
    }

    //==============================================================================
    //  The three send effects, as in Kitbox (0.2.0). Each slot holds one
    //  category of Rackbox's catalog and Type picks the effect within it; A and
    //  B are that effect's two knobs, 0..1, named and printed by EffectCatalog.
    namespace Fx
    {
        constexpr int numSlots = 3;
        enum Slot { reverb, delay, mod };

        struct Ids { const char* type; const char* a; const char* b; const char* level; };

        inline constexpr Ids ids[numSlots] {
            { "rev_type", "rev_a", "rev_b", "rev_level" },
            { "dly_type", "dly_a", "dly_b", "dly_level" },
            { "mod_type", "mod_a", "mod_b", "mod_level" }
        };

        inline constexpr const char* titles[numSlots] { "Reverb", "Delay", "Mod" };

        /** Where each slot sits in EffectCatalog (Modulation 0, Reverb 1, Delay 2). */
        inline constexpr int categories[numSlots] { 1, 2, 0 };

        /** Kitbox's defaults: a plate, a tape delay at 1/8 and a phaser. */
        inline constexpr int   defaultTypes[numSlots] { 0, 1, 1 };                     // Plate, Tape, Phaser
        inline constexpr float defaultA[numSlots]     { 0.5f, 5.0f / 13.0f, 0.392f };   // 50 %, 1/8, 0.4 Hz
        inline constexpr float defaultB[numSlots]     { 0.4f, 0.37f, 0.7f };            // 40 %, 35 % feedback, 70 %
    }
}
