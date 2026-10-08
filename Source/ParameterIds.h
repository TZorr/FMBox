//
//  ParameterIds.h
//  FMBox
//
//  Every parameter the host can see, and the eight sounds they belong to.
//
//  A sound's parameters are the same six suffixes eight times over, with the
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
    };

    inline constexpr SoundSpec sounds[numSounds] = {
        { "bd",   "Kick",       "Tone",   36,   30.0f,  150.0f,   50.0f, 0.55f, 0.55f, 0.45f },
        { "sd",   "Snare",      "Snappy", 38,  100.0f,  450.0f,  200.0f, 0.35f, 0.25f, 0.60f },
        { "cp",   "Clap",       "Spread", 39,  500.0f, 3000.0f, 1150.0f, 0.35f, 0.20f, 0.45f },
        { "ch",   "Closed Hat", "Tone",   42,  100.0f, 1200.0f,  300.0f, 0.40f, 0.30f, 0.80f },
        { "oh",   "Open Hat",   "Tone",   46,  100.0f, 1200.0f,  300.0f, 0.55f, 0.30f, 0.75f },
        { "tom",  "Tom",        "Bend",   45,   50.0f,  400.0f,  120.0f, 0.40f, 0.30f, 0.40f },
        { "rim",  "Rim",        "Click",  37,  300.0f, 2500.0f, 1100.0f, 0.20f, 0.45f, 0.50f },
        { "bell", "Bell",       "Ratio",  56,  150.0f, 2000.0f,  560.0f, 0.22f, 0.35f, 0.137f },
    };

    /** The Bell's Tone knob is the modulator ratio, 1.0 .. 4.0. */
    inline float bellRatio (float tone) { return 1.0f + 3.0f * tone; }

    /** The sound a MIDI note plays, or -1. */
    inline int soundForNote (int note)
    {
        for (int s = 0; s < numSounds; ++s)
            if (sounds[s].note == note)
                return s;
        return -1;
    }

    //==============================================================================
    //  Per sound
    namespace Snd
    {
        inline constexpr const char* tune  = "tune";
        inline constexpr const char* decay = "decay";
        inline constexpr const char* fm    = "fm";
        inline constexpr const char* tone  = "tone";
        inline constexpr const char* level = "level";
        inline constexpr const char* pan   = "pan";

        inline constexpr const char* all[] = { tune, decay, fm, tone, level, pan };
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
}
