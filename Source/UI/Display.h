//
//  Display.h
//  FMBox
//
//  The dark display in the header, as on Kitbox and Rackbox Plus: the sound
//  played last on the left - its name, note and velocity - and the three send
//  effects on the right, each with the effect loaded and its return level.
//
//  Top right sits LEARN, a small switch drawn like Chordbox's RND: it starts
//  MIDI Learn for the selected pad - the next key played becomes its note -
//  and a second click cancels. While it waits, the left half says so.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FMBoxProcessor;

class Display : public juce::Component
{
public:
    explicit Display (FMBoxProcessor& processorToUse);

    /** 30 Hz, from the editor: repaints only when something shown changed. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class LearnButton : public juce::Button
    {
    public:
        LearnButton() : juce::Button ("LEARN") {}
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    };

    juce::String fxLine (int slot) const;

    FMBoxProcessor& processor;
    int shownSound = -2, shownHits = -1, shownLearn = -2, shownNote = -1;
    LearnButton learn;
    juce::StringArray shownFx;
};
