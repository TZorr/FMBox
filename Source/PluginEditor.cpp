//
//  PluginEditor.cpp
//  FMBox
//

#include "PluginEditor.h"
#include "UI/Palette.h"

namespace
{
    juce::Rectangle<int> columnArea (int index)
    {
        return { FMBoxEditor::margin + index * FMBoxEditor::columnWidth,
                 FMBoxEditor::headerHeight,
                 FMBoxEditor::columnWidth,
                 FMBoxEditor::padHeight + 3 * FMBoxEditor::knobRowHeight + 22 };
    }
}

FMBoxEditor::FMBoxEditor (FMBoxProcessor& p)
    : AudioProcessorEditor (p), fmProcessor (p)
{
    using namespace FmParams;

    for (int s = 0; s < numSounds; ++s)
    {
        auto& column = columns[(size_t) s];
        column.pad = std::make_unique<PadButton> (fmProcessor, s);
        addAndMakeVisible (*column.pad);

        const char* labels[] = { "Tune", "Decay", "FM", sounds[s].toneLabel, "Level", "Pan" };

        for (size_t k = 0; k < column.knobs.size(); ++k)
        {
            auto knob = std::make_unique<Knob> (labels[k], k == 5);
            knob->attach (fmProcessor.parameters, soundId (s, Snd::all[k]));
            addAndMakeVisible (*knob);
            column.knobs[k] = std::move (knob);
        }
    }

    masterKnob.attach (fmProcessor.parameters, FmParams::master);
    addAndMakeVisible (masterKnob);

    setSize (2 * margin + numSounds * columnWidth,
             columnArea (0).getBottom() + margin);

    startTimerHz (30);
}

FMBoxEditor::~FMBoxEditor()
{
    stopTimer();
}

void FMBoxEditor::timerCallback()
{
    for (auto& column : columns)
        column.pad->updateFlash();
}

void FMBoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::body);

    // The header: the name in the display's colours, the master on the right.
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin, 12);
    auto display = header.removeFromLeft (300);

    g.setColour (Palette::display);
    g.fillRoundedRectangle (display.toFloat(), 6.0f);

    auto text = display.reduced (14, 6);
    g.setColour (Palette::displayText);
    g.setFont (Palette::mono (22.0f, true));
    g.drawText ("FMBOX", text.removeFromLeft (100), juce::Justification::centredLeft);

    g.setColour (Palette::displayDim);
    g.setFont (Palette::mono (10.5f));
    g.drawText ("8 FM DRUMS  /  GM NOTES", text, juce::Justification::centredLeft);

    // A recessed section per sound.
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        const auto area = columnArea (s).reduced (3, 0).toFloat();
        g.setColour (Palette::section);
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (Palette::sectionLine);
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);
    }
}

void FMBoxEditor::resized()
{
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        auto area = columnArea (s).reduced (9, 9);
        auto& column = columns[(size_t) s];

        column.pad->setBounds (area.removeFromTop (padHeight));
        area.removeFromTop (4);

        for (int row = 0; row < 3; ++row)
        {
            auto line = area.removeFromTop (knobRowHeight);
            const auto half = line.getWidth() / 2;
            column.knobs[(size_t) row * 2]    ->setBounds (line.removeFromLeft (half));
            column.knobs[(size_t) row * 2 + 1]->setBounds (line);
        }
    }

    masterKnob.setBounds (getWidth() - margin - 70, 4, 64, headerHeight - 6);
}
