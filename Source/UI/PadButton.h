//
//  PadButton.h
//  FMBox
//
//  The pad at the bottom of each sound's column: its name, its note, and a
//  flash when it plays - from a click or from MIDI.
//
//  Click to select the sound - Learn assigns its note - and to audition it,
//  louder the higher up it is hit: the top edge is full velocity, the bottom
//  edge about a third.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class FMBoxProcessor;

class PadButton : public juce::Component
{
public:
    PadButton (FMBoxProcessor& processorToUse, int soundIndex);

    /** 30 Hz, from the editor: picks up hits, the selection and the note, and
        fades the flash. */
    void updateFlash();

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    FMBoxProcessor& processor;
    const int sound;
    int lastHitCount = 0, shownNote = -1;
    bool selected = false;
    float flash = 0.0f;
};
