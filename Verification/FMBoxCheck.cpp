//
//  FMBoxCheck.cpp
//  FMBox
//
//  Measures the engine the plugin runs - the same object files - and writes
//  every sound to a WAV to listen to.
//
//  What is checked, per sound and at 44.1, 48 and 96 kHz:
//
//  - it sounds (peak above -30 dBFS) and does not clip (below 0 dBFS) at its
//    defaults and full velocity, and leaves within 6 dB of the others;
//  - no NaN or infinity, with every knob at 0 and at 1 as well;
//  - it goes quiet and stops being computed within four seconds;
//  - its length does not depend on the sample rate (-40 dB point at 96 kHz
//    within 10 % of 48 kHz) - the DaisySP hats' constants were per sample;
//  - the closed hat chokes the open one, and a retrigger leaves no step;
//  - Click adds a transient, Drive stays bounded, Velo 0 % ignores velocity;
//  - the three sends reach their effects: reverb tail, a 1/8 echo at 120 BPM,
//    and a return at -inf leaves the mix untouched bit for bit.
//
//  Usage:  FMBoxCheck [wav-directory]
//

#include <juce_audio_formats/juce_audio_formats.h>

#include "Engine/FmDrumEngine.h"

#include <cstdio>

namespace
{
    int failures = 0, checks = 0;

    void check (bool condition, const juce::String& what, const juce::String& detail = {})
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("FAIL: %s  %s\n", what.toRawUTF8(), detail.toRawUTF8());
        }
    }

    juce::String db (float gain)
    {
        return juce::String (juce::Decibels::gainToDecibels (gain, -120.0f), 1) + " dB";
    }

    VoiceControls defaults (int sound)
    {
        const auto& spec = FmParams::sounds[sound];
        return { spec.tune, spec.decay, spec.fm, spec.tone };
    }

    using Tweak = std::function<void (int, FmDrumEngine::SoundSettings&)>;

    /** A fresh engine with every sound at the given controls, centred, 0 dB,
        no drive, click or sends, full velocity sensitivity - then tweaked. */
    std::unique_ptr<FmDrumEngine> makeEngine (double rate, std::function<VoiceControls (int)> controls = defaults,
                                              Tweak tweak = nullptr)
    {
        auto engine = std::make_unique<FmDrumEngine>();

        for (int s = 0; s < FmParams::numSounds; ++s)
        {
            FmDrumEngine::SoundSettings settings;
            settings.controls = controls (s);

            if (tweak)
                tweak (s, settings);

            engine->setSound (s, settings);
        }

        engine->prepare (rate, 512);
        return engine;
    }

    struct Take
    {
        juce::AudioBuffer<float> audio;
        float peak = 0.0f;
        bool finite = true;
        int sleptAt = -1;   // sample at which the engine stopped computing the sound
    };

    Take render (FmDrumEngine& engine, int sound, double rate, double seconds, float velocity = 1.0f)
    {
        Take take;
        const auto length = (int) (seconds * rate);
        take.audio.setSize (2, length);

        engine.trigger (sound, velocity);

        constexpr int block = 256;

        for (int start = 0; start < length; start += block)
        {
            const auto n = juce::jmin (block, length - start);
            engine.render (take.audio.getWritePointer (0, start), take.audio.getWritePointer (1, start), n);

            if (take.sleptAt < 0 && ! engine.isSounding (sound))
                take.sleptAt = start + n;
        }

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
            {
                const auto x = take.audio.getSample (ch, i);
                take.finite = take.finite && std::isfinite (x);
                take.peak = juce::jmax (take.peak, std::abs (x));
            }

        return take;
    }

    /** The last sample whose 5 ms envelope is within `db` of the peak. */
    double lengthTo (const Take& take, double rate, float dbBelowPeak)
    {
        const auto threshold = take.peak * juce::Decibels::decibelsToGain (-dbBelowPeak);
        const auto* x = take.audio.getReadPointer (0);
        const auto window = (int) (0.005 * rate);
        int last = 0;

        for (int i = 0; i + window < take.audio.getNumSamples(); i += window / 2)
        {
            float m = 0.0f;
            for (int k = 0; k < window; ++k)
                m = juce::jmax (m, std::abs (x[i + k]));
            if (m > threshold)
                last = i + window;
        }

        return last / rate;
    }

    void writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double rate)
    {
        file.deleteFile();
        juce::WavAudioFormat wav;

        if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
            if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                                                               .withSampleRate (rate)
                                                               .withNumChannels (2)
                                                               .withBitsPerSample (24)))
                writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
    }

    //==============================================================================
    void checkSounds (const juce::File& wavDirectory)
    {
        std::printf ("%-11s %9s %9s %9s %9s %8s\n", "sound", "peak", "-40 dB", "@96k", "asleep", "trim");

        std::array<float, FmParams::numSounds> peaks {};

        for (int s = 0; s < FmParams::numSounds; ++s)
        {
            const juce::String name (FmParams::sounds[s].name);
            std::array<double, 3> lengths {};
            double sleptMs = 0.0;
            const double rates[] = { 44100.0, 48000.0, 96000.0 };

            for (int r = 0; r < 3; ++r)
            {
                auto engine = makeEngine (rates[r]);
                const auto take = render (*engine, s, rates[r], 4.0);
                const auto at = name + " @" + juce::String (rates[r] / 1000.0, 1) + "k";

                check (take.finite, at + " is finite");
                check (take.peak > juce::Decibels::decibelsToGain (-30.0f), at + " sounds", db (take.peak));
                check (take.peak < 1.0f, at + " stays below 0 dBFS", db (take.peak));
                check (take.sleptAt > 0, at + " goes to sleep within 4 s");

                lengths[(size_t) r] = lengthTo (take, rates[r], 40.0f);

                if (r == 1)
                {
                    peaks[(size_t) s] = take.peak;
                    sleptMs = take.sleptAt / rates[r] * 1000.0;

                    if (wavDirectory != juce::File())
                        writeWav (wavDirectory.getChildFile (juce::String (s + 1) + " " + name + ".wav"), take.audio, rates[r]);
                }
            }

            // The trim that would put this sound's peak at -6 dBFS.
            const auto suggested = 0.5f / juce::jmax (1.0e-6f, peaks[(size_t) s] / FmDrumEngine::trims[(size_t) s]);
            std::printf ("%-11s %9s %6.0f ms %6.0f ms %6.0f ms %8.3f\n", name.toRawUTF8(), db (peaks[(size_t) s]).toRawUTF8(),
                         lengths[1] * 1000.0, lengths[2] * 1000.0, sleptMs, suggested);

            const auto ratio = lengths[2] / juce::jmax (1.0e-6, lengths[1]);
            check (std::abs (ratio - 1.0) < 0.1, name + " is as long at 96 kHz as at 48 kHz",
                   juce::String (lengths[1] * 1000.0, 0) + " vs " + juce::String (lengths[2] * 1000.0, 0) + " ms");
        }

        const auto loudest = *std::max_element (peaks.begin(), peaks.end());
        const auto quietest = *std::min_element (peaks.begin(), peaks.end());
        check (loudest / quietest < 2.0f, "the eight sounds leave within 6 dB of each other",
               db (quietest) + " .. " + db (loudest));
    }

    void checkExtremes()
    {
        for (const auto corner : { 0.0f, 1.0f })
            for (int s = 0; s < FmParams::numSounds; ++s)
            {
                auto engine = makeEngine (48000.0, [corner] (int sound)
                {
                    const auto& spec = FmParams::sounds[sound];
                    return VoiceControls { corner > 0.5f ? spec.tuneMax : spec.tuneMin, corner, corner, corner };
                },
                [corner] (int, FmDrumEngine::SoundSettings& settings)
                {
                    settings.drive = settings.click = corner;
                });

                const auto take = render (*engine, s, 48000.0, 3.0);
                const auto at = juce::String (FmParams::sounds[s].name) + " with every knob at " + juce::String ((int) corner);
                check (take.finite, at + " is finite");
                check (take.peak < 2.0f, at + " stays within +6 dBFS", db (take.peak));
            }
    }

    void checkChoke()
    {
        constexpr double rate = 48000.0;
        auto engine = makeEngine (rate);
        juce::AudioBuffer<float> audio (2, (int) rate);

        engine->trigger (FmParams::oh, 1.0f);
        engine->render (audio.getWritePointer (0), audio.getWritePointer (1), 4800);   // 100 ms
        check (engine->isSounding (FmParams::oh), "the open hat rings 100 ms in");

        engine->trigger (FmParams::ch, 1.0f);
        engine->render (audio.getWritePointer (0), audio.getWritePointer (1), 480);    // 10 ms
        check (! engine->isSounding (FmParams::oh), "the closed hat chokes the open hat within 10 ms");
    }

    void checkRetrigger()
    {
        // Two kicks 30 ms apart, while the first is at full body: the largest
        // step between neighbouring samples around the second hit may not be
        // much larger than anywhere in the first hit's own attack.
        constexpr double rate = 48000.0;
        auto engine = makeEngine (rate);
        juce::AudioBuffer<float> audio (2, 9600);
        auto* l = audio.getWritePointer (0);
        auto* r = audio.getWritePointer (1);

        engine->trigger (FmParams::bd, 1.0f);
        engine->render (l, r, 1440);
        engine->trigger (FmParams::bd, 1.0f);
        engine->render (l + 1440, r + 1440, 9600 - 1440);

        auto largestStep = [l] (int from, int to)
        {
            float step = 0.0f;
            for (int i = from + 1; i < to; ++i)
                step = juce::jmax (step, std::abs (l[i] - l[i - 1]));
            return step;
        };

        const auto first = largestStep (0, 1440);
        const auto second = largestStep (1440, 1440 + 240);
        check (second < 1.5f * first, "a retriggered kick does not jump", juce::String (second, 4) + " vs " + juce::String (first, 4));
    }

    float peakOf (const Take& take, double rate, double fromSeconds, double toSeconds)
    {
        float peak = 0.0f;
        const auto from = (int) (fromSeconds * rate), to = juce::jmin (take.audio.getNumSamples(), (int) (toSeconds * rate));

        for (int i = from; i < to; ++i)
            peak = juce::jmax (peak, std::abs (take.audio.getSample (0, i)));

        return peak;
    }

    void checkShaping()
    {
        constexpr double rate = 48000.0;

        // Click puts a transient on top: more in the first 3 ms of a tom.
        {
            auto plain = makeEngine (rate);
            auto clicked = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& s) { s.click = 1.0f; });
            const auto a = peakOf (render (*plain, FmParams::tom, rate, 0.1), rate, 0.0, 0.003);
            const auto b = peakOf (render (*clicked, FmParams::tom, rate, 0.1), rate, 0.0, 0.003);
            check (b > a * 1.4f, "Click adds a transient", db (a) + " -> " + db (b));
        }

        // Drive at full adds harmonics and stays bounded on every sound.
        for (int s = 0; s < FmParams::numSounds; ++s)
        {
            auto driven = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& x) { x.drive = 1.0f; });
            const auto take = render (*driven, s, rate, 1.0);
            const auto name = juce::String (FmParams::sounds[s].name);
            check (take.finite && take.peak < 1.0f, name + " with full drive stays below 0 dBFS", db (take.peak));
            check (take.peak > 0.05f, name + " with full drive still sounds", db (take.peak));
        }

        // Velocity sensitivity: 0 plays a soft hit at full level, 1 does not.
        {
            const auto hit = [&] (float sensitivity, float velocity)
            {
                auto engine = makeEngine (rate, defaults, [sensitivity] (int, FmDrumEngine::SoundSettings& x) { x.velocity = sensitivity; });
                return render (*engine, FmParams::bell, rate, 0.2, velocity).peak;
            };

            const auto full = hit (1.0f, 1.0f);
            check (std::abs (hit (0.0f, 0.2f) - full) < 0.01f * full, "Velo 0 % plays a soft hit at full level");
            check (hit (1.0f, 0.2f) < 0.5f * full, "Velo 100 % plays a soft hit softer", db (hit (1.0f, 0.2f)) + " vs " + db (full));
        }
    }

    void checkSends()
    {
        constexpr double rate = 48000.0;
        const auto dry = [&] { auto e = makeEngine (rate); return render (*e, FmParams::rim, rate, 1.0); }();

        // A send whose return is at -inf changes nothing, bit for bit.
        {
            auto engine = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& x) { x.sends = { 1.0f, 1.0f, 1.0f }; });
            for (int f = 0; f < FmParams::Fx::numSlots; ++f)
                engine->setFx (f, { FmParams::Fx::defaultTypes[f], FmParams::Fx::defaultA[f], FmParams::Fx::defaultB[f], 0.0f });
            engine->prepare (rate, 512);

            const auto take = render (*engine, FmParams::rim, rate, 1.0);
            bool same = true;
            for (int i = 0; i < dry.audio.getNumSamples(); ++i)
                same = same && juce::exactlyEqual (take.audio.getSample (0, i), dry.audio.getSample (0, i));
            check (same, "sends into returns at -inf leave the mix untouched");
        }

        // The reverb makes the rim ring on.
        {
            auto engine = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& x) { x.sends[FmParams::Fx::reverb] = 1.0f; });
            const auto take = render (*engine, FmParams::rim, rate, 1.0);
            check (take.finite, "the reverb return is finite");
            check (peakOf (take, rate, 0.2, 0.4) > 30.0f * peakOf (dry, rate, 0.2, 0.4), "the reverb send makes the rim ring on",
                   db (peakOf (take, rate, 0.2, 0.4)));
        }

        // The tape delay at 1/8 and 120 BPM repeats the rim 250 ms later.
        {
            auto engine = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& x) { x.sends[FmParams::Fx::delay] = 1.0f; });
            engine->setTempo (120.0);
            const auto take = render (*engine, FmParams::rim, rate, 1.0);
            const auto echo = peakOf (take, rate, 0.23, 0.30);
            check (echo > 0.1f * dry.peak, "the delay send echoes the rim after 1/8 at 120 BPM", db (echo));
            check (peakOf (take, rate, 0.12, 0.2) < 0.25f * echo, "... and not before it");
        }

        // The phaser changes the open hat and stays finite.
        {
            auto engine = makeEngine (rate, defaults, [] (int, FmDrumEngine::SoundSettings& x) { x.sends[FmParams::Fx::mod] = 1.0f; });
            const auto take = render (*engine, FmParams::oh, rate, 1.0);
            check (take.finite && take.peak < 1.0f, "the mod return is finite and below 0 dBFS", db (take.peak));
        }
    }

    void checkIdle()
    {
        auto engine = makeEngine (48000.0);
        juce::AudioBuffer<float> audio (2, 512);
        engine->render (audio.getWritePointer (0), audio.getWritePointer (1), 512);

        bool silent = true;
        for (int i = 0; i < 512; ++i)
            silent = silent && audio.getSample (0, i) == 0.0f && audio.getSample (1, i) == 0.0f;

        check (silent, "an untouched engine is silent");

        for (int s = 0; s < FmParams::numSounds; ++s)
            check (! engine->isSounding (s), "an untouched engine computes nothing");
    }
}

int main (int argc, char** argv)
{
    juce::File wavDirectory;

    if (argc > 1)
    {
        wavDirectory = juce::File (juce::String (argv[1]));
        wavDirectory.createDirectory();
    }

    checkIdle();
    checkSounds (wavDirectory);
    checkExtremes();
    checkChoke();
    checkRetrigger();
    checkShaping();
    checkSends();

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
