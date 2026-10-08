//
//  Display.h
//  FMBox
//
//  The dark display in the header, as on Kitbox and Rackbox Plus: the sound
//  played last on the left - its name, note and velocity - and the three send
//  effects on the right, each with the effect loaded and its return level.
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

private:
    juce::String fxLine (int slot) const;

    FMBoxProcessor& processor;
    int shownSound = -2, shownHits = -1;
    juce::StringArray shownFx;
};
