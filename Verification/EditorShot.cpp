//
//  EditorShot.cpp
//  FMBox
//
//  Builds the plugin headlessly, checks what needs the whole processor, and
//  paints the panel into a PNG.
//
//  - the session round trip: every parameter through getStateInformation
//    into a fresh processor, with the knobs moved immediately before saving -
//    which is when a state saved from the tree instead of copyState() comes
//    back stale;
//  - every GM note plays its sound, and on the sample the MIDI event names;
//  - a click on a pad plays that sound.
//
//  Look at the PNG after every layout change.
//
//  Usage:  EditorShot <output-directory>
//

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "Engine/PresetFile.h"
#include "UI/Palette.h"
#include "PluginProcessor.h"

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

    void setParam (FMBoxProcessor& processor, const juce::String& id, float value)
    {
        auto* parameter = processor.parameters.getParameter (id);
        jassert (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    float getParam (FMBoxProcessor& processor, const juce::String& id)
    {
        return processor.parameters.getRawParameterValue (id)->load();
    }

    bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        juce::PNGImageFormat png;
        return stream != nullptr && png.writeImageToStream (image, *stream);
    }

    void pump (int ms)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
    }

    //==============================================================================
    void checkStateRoundTrip()
    {
        FMBoxProcessor original;
        original.prepareToPlay (48000.0, 512);

        setParam (original, FmParams::soundId (FmParams::sd, FmParams::Snd::tune), 260.0f);
        setParam (original, FmParams::soundId (FmParams::bell, FmParams::Snd::tone), 0.8f);
        setParam (original, FmParams::soundId (FmParams::ch, FmParams::Snd::pan), -0.4f);
        setParam (original, FmParams::soundId (FmParams::bd, FmParams::Snd::level), -9.0f);
        setParam (original, FmParams::master, -3.0f);
        setParam (original, FmParams::soundId (FmParams::tom, FmParams::Snd::drive), 0.6f);
        setParam (original, FmParams::soundId (FmParams::rim, FmParams::Snd::delay), 0.4f);
        setParam (original, FmParams::Fx::ids[FmParams::Fx::reverb].type, 2.0f);
        setParam (original, FmParams::Fx::ids[FmParams::Fx::mod].b, 0.25f);
        setParam (original, FmParams::soundId (FmParams::bell, FmParams::Snd::note), 60.0f);

        juce::MemoryBlock state;
        original.getStateInformation (state);

        FMBoxProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());

        int compared = 0, differing = 0;

        for (auto* parameter : original.getParameters())
            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            {
                ++compared;
                const auto a = getParam (original, withId->paramID);
                const auto b = getParam (restored, withId->paramID);

                if (std::abs (a - b) > 1.0e-4f)
                {
                    ++differing;
                    std::printf ("  %s: %f -> %f\n", withId->paramID.toRawUTF8(), a, b);
                }
            }

        check (compared == FmParams::numSounds * (FmParams::Snd::count + 1) + FmParams::Fx::numSlots * 4 + 1,
               "every parameter is in the state", juce::String (compared));
        check (differing == 0, "the session round trip restores every parameter", juce::String (differing) + " differ");
    }

    /** First sample in the block above -100 dB, or -1. */
    int firstSound (const juce::AudioBuffer<float>& buffer)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (std::abs (buffer.getSample (0, i)) > 1.0e-5f || std::abs (buffer.getSample (1, i)) > 1.0e-5f)
                return i;
        return -1;
    }

    void checkMidi()
    {
        for (int s = 0; s < FmParams::numSounds; ++s)
        {
            FMBoxProcessor processor;
            processor.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (10, FmParams::sounds[s].note, (juce::uint8) 127), 300);
            processor.processBlock (buffer, midi);

            const auto first = firstSound (buffer);
            const auto name = juce::String (FmParams::sounds[s].name);
            check (processor.getEngine().getHitCount (s) == 1, name + " plays from note " + juce::String (FmParams::sounds[s].note));
            check (first >= 300 && first <= 303, name + " starts on the sample the note is on", juce::String (first));
        }

        FMBoxProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
        processor.processBlock (buffer, midi);
        check (firstSound (buffer) < 0, "a note outside the kit plays nothing");

        processor.playSound (FmParams::rim, 0.8f);
        midi.clear();
        processor.processBlock (buffer, midi);
        check (processor.getEngine().getHitCount (FmParams::rim) == 1 && firstSound (buffer) == 0,
               "a pad click plays its sound at the start of the next block");
    }
}

namespace
{
    /** One block with one note-on at sample 0. */
    void playNote (FMBoxProcessor& processor, int note)
    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        processor.processBlock (buffer, midi);
    }

    void checkLearn()
    {
        using namespace FmParams;

        FMBoxProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        // A free note: the tom takes 60, nobody else changes.
        processor.setSelectedSound (tom);
        processor.startLearn (processor.getSelectedSound());
        check (processor.getLearnSound() == tom, "Learn waits for the selected pad");
        playNote (processor, 60);
        processor.applyLearnedNote();
        check (processor.getLearnSound() < 0, "Learn stops after one note");
        check (processor.getSoundNote (tom) == 60, "the tom learns note 60", juce::String (processor.getSoundNote (tom)));
        check (processor.getEngine().getHitCount (tom) == 0, "the learning note still plays only what it played before");

        // A taken note: kick and snare swap.
        processor.startLearn (bd);
        playNote (processor, 38);
        processor.applyLearnedNote();
        check (processor.getSoundNote (bd) == 38 && processor.getSoundNote (sd) == 36, "learning the snare's note swaps kick and snare",
               juce::String (processor.getSoundNote (bd)) + " / " + juce::String (processor.getSoundNote (sd)));

        const auto kickHits = processor.getEngine().getHitCount (bd), snareHits = processor.getEngine().getHitCount (sd);
        playNote (processor, 38);
        check (processor.getEngine().getHitCount (bd) == kickHits + 1 && processor.getEngine().getHitCount (sd) == snareHits,
               "note 38 now plays the kick, not the snare");

        // Cancelled: a note changes nothing.
        processor.startLearn (rim);
        processor.cancelLearn();
        playNote (processor, 72);
        processor.applyLearnedNote();
        check (processor.getSoundNote (rim) == sounds[rim].note, "a cancelled Learn changes nothing");
    }
}

namespace
{
    /** The preset menu's own file: saves and loads the knobs, is readable XML
        naming this plugin, and refuses another plugin's preset and an
        .aupreset. */
    void checkPresets()
    {
        const auto folder = juce::File::createTempFile ("presets");
        folder.createDirectory();
        const auto file = folder.getChildFile (juce::String ("Test") + PresetFile::extension);

        FMBoxProcessor original;
        setParam (original, FmParams::soundId (FmParams::sd, FmParams::Snd::tune), 260.0f);
        check (PresetButton::save (original, file).wasOk(), "a preset saves");

        const auto text = file.loadFileAsString();
        const auto xml = juce::XmlDocument::parse (text);
        check (xml != nullptr && xml->hasTagName ("TZorrPreset") && xml->getStringAttribute ("plugin") == "Fmbx"
                   && xml->getNumChildElements() == 1,
               "the preset is readable XML naming Fmbx", text.substring (0, 120));

        FMBoxProcessor loaded;
        check (PresetButton::load (loaded, file).wasOk(), "a preset loads");
        check (std::abs (getParam (loaded, FmParams::soundId (FmParams::sd, FmParams::Snd::tune)) - 260.0f) < 0.5f,
               "a preset brings back the knobs", juce::String (getParam (loaded, FmParams::soundId (FmParams::sd, FmParams::Snd::tune))));

        const auto foreign = folder.getChildFile (juce::String ("Foreign") + PresetFile::extension);
        foreign.replaceWithText (text.replace ("plugin=\"Fmbx\"", "plugin=\"Tnbx\""));
        FMBoxProcessor untouched;
        const auto before = getParam (untouched, FmParams::soundId (FmParams::sd, FmParams::Snd::tune));
        check (PresetButton::load (untouched, foreign).failed() && getParam (untouched, FmParams::soundId (FmParams::sd, FmParams::Snd::tune)) == before,
               "another plugin's preset is refused");

        const auto auPreset = folder.getChildFile ("Logic.aupreset");
        auPreset.replaceWithText ("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict/></plist>");
        check (PresetButton::load (untouched, auPreset).failed() && getParam (untouched, FmParams::soundId (FmParams::sd, FmParams::Snd::tune)) == before,
               "an .aupreset is refused");

        folder.deleteRecursively();
    }
}

int main (int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;

    const juce::File outputDirectory = argc > 1 ? juce::File (juce::String (argv[1]))
                                                : juce::File::getCurrentWorkingDirectory();

    if (! outputDirectory.createDirectory())
    {
        std::printf ("FAIL: cannot create %s\n", outputDirectory.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("Session round trip...\n");
    checkStateRoundTrip();
    std::printf ("Presets...\n");
    checkPresets();
    std::printf ("MIDI notes and pads...\n");
    checkMidi();
    std::printf ("Learn...\n");
    checkLearn();

    // The panel, with a few pads mid-flash.
    {
        FMBoxProcessor processor;
        processor.prepareToPlay (48000.0, 512);
        setParam (processor, FmParams::soundId (FmParams::ch, FmParams::Snd::pan), -0.3f);
        setParam (processor, FmParams::soundId (FmParams::oh, FmParams::Snd::pan), 0.3f);
        setParam (processor, FmParams::soundId (FmParams::bd, FmParams::Snd::drive), 0.35f);
        setParam (processor, FmParams::soundId (FmParams::bd, FmParams::Snd::click), 0.4f);
        setParam (processor, FmParams::soundId (FmParams::rim, FmParams::Snd::delay), 0.3f);
        setParam (processor, FmParams::Fx::ids[FmParams::Fx::reverb].type, 1.0f);   // Room

        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorAndMakeActive());
        pump (50);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
        midi.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 80), 10);
        processor.processBlock (buffer, midi);

        // Then Learn waiting on the bell, so the panel shows that state.
        processor.setSelectedSound (FmParams::bell);
        processor.startLearn (FmParams::bell);

        pump (60);

        // Whatever theme this Mac's user picked, the reference shot is light;
        // the dark one beside it. Palette::setDark, not the button, so the
        // user's setting is left alone.
        Palette::setDark (false);
        editor->repaint();
        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        const auto file = outputDirectory.getChildFile ("fmbox.png");
        check (writePng (image, file), "the panel renders to " + file.getFullPathName());

        Palette::setDark (true);
        editor->repaint();
        check (writePng (editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f),
                         outputDirectory.getChildFile ("fmbox-dark.png")), "the panel renders dark");
        Palette::setDark (false);
        editor->repaint();
        std::printf ("Panel: %d x %d\n", editor->getWidth(), editor->getHeight());

        processor.editorBeingDeleted (editor.get());
        editor.reset();
    }

    std::printf ("\n%d checks, %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
