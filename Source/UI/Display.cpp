//
//  Display.cpp
//  FMBox
//

#include "Display.h"
#include "Palette.h"
#include "PluginProcessor.h"

namespace
{
    const juce::String dot = juce::String::charToString (0x00b7);
}

Display::Display (FMBoxProcessor& processorToUse) : processor (processorToUse)
{
    setInterceptsMouseClicks (false, true);

    learn.onClick = [this]
    {
        if (processor.getLearnSound() >= 0)
            processor.cancelLearn();
        else
            processor.startLearn (processor.getSelectedSound());

        refresh();
    };

    learn.setTooltip ("Learn: click a pad, press LEARN, play a key - the key becomes the pad's note");
    learn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (learn);
}

void Display::LearnButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto on = getToggleState();

    g.setColour (on ? Palette::accent : (highlighted ? Palette::displayGrid.brighter (0.2f) : Palette::displayGrid));
    g.fillRoundedRectangle (bounds, 3.0f);

    g.setColour (on ? Palette::display : Palette::displayDim);
    g.setFont (Palette::mono (9.5f, true));
    g.drawText ("LEARN", bounds, juce::Justification::centred);
}

void Display::resized()
{
    // Top right, level with the section captions.
    learn.setBounds (getWidth() - 18 - 46, 12, 46, 15);
}

juce::String Display::fxLine (int slot) const
{
    const auto& ids = FmParams::Fx::ids[slot];
    const auto& state = processor.parameters;
    const auto category = FmParams::Fx::categories[slot];
    const auto type = EffectCatalog::clampSubtype (category, (int) std::lround (state.getRawParameterValue (ids.type)->load()));
    const auto* level = state.getParameter (ids.level);

    return juce::String (FmParams::Fx::titles[slot]).toUpperCase() + "  " + dot + "  "
         + juce::String (EffectCatalog::info (category, type).name).toUpperCase() + "  " + dot + "  "
         + level->getCurrentValueAsText();
}

void Display::refresh()
{
    const auto& engine = processor.getEngine();
    const auto sound = engine.getLastSound();
    const auto hits = sound >= 0 ? engine.getHitCount (sound) : 0;
    const auto learning = processor.getLearnSound();
    const auto note = sound >= 0 ? processor.getSoundNote (sound) : -1;

    if (learn.getToggleState() != (learning >= 0))
        learn.setToggleState (learning >= 0, juce::dontSendNotification);

    juce::StringArray fx;
    for (int slot = 0; slot < FmParams::Fx::numSlots; ++slot)
        fx.add (fxLine (slot));

    if (sound != shownSound || hits != shownHits || fx != shownFx || learning != shownLearn || note != shownNote)
    {
        shownSound = sound;
        shownHits = hits;
        shownLearn = learning;
        shownNote = note;
        shownFx = fx;
        repaint();
    }
}

void Display::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (Palette::display);
    g.fillRoundedRectangle (bounds, 6.0f);

    auto area = bounds.reduced (18.0f, 12.0f);

    // Left: the sound played last.
    auto left = area.removeFromLeft (area.getWidth() * 0.42f);

    if (shownLearn >= 0)
    {
        // Waiting for a key.
        g.setFont (Palette::label (10.0f));
        g.setColour (Palette::accent);
        g.drawText ("LEARN", left.removeFromTop (16.0f), juce::Justification::centredLeft);

        g.setFont (Palette::mono (22.0f, true));
        g.setColour (Palette::displayText);
        g.drawText (juce::String (FmParams::sounds[shownLearn].name).toUpperCase(), left.removeFromTop (32.0f), juce::Justification::centredLeft);

        g.setFont (Palette::mono (11.0f));
        g.setColour (Palette::displayDim);
        g.drawText ("play a key ...", left.removeFromTop (18.0f), juce::Justification::centredLeft);
    }
    else
    {
        g.setFont (Palette::label (10.0f));
        g.setColour (Palette::displayDim);
        g.drawText ("LAST HIT", left.removeFromTop (16.0f), juce::Justification::centredLeft);

        if (shownSound >= 0)
        {
            const auto& spec = FmParams::sounds[shownSound];
            const auto velocity = juce::roundToInt (processor.getEngine().getLastVelocity (shownSound) * 127.0f);

            g.setFont (Palette::mono (22.0f, true));
            g.setColour (Palette::displayText);
            g.drawText (juce::String (spec.name).toUpperCase(), left.removeFromTop (32.0f), juce::Justification::centredLeft);

            g.setFont (Palette::mono (11.0f));
            g.setColour (Palette::displayDim);
            g.drawText (FmParams::noteName (shownNote) + "  " + dot + "  note " + juce::String (shownNote)
                            + "  " + dot + "  vel " + juce::String (velocity),
                        left.removeFromTop (18.0f), juce::Justification::centredLeft);
        }
        else
        {
            g.setFont (Palette::mono (22.0f, true));
            g.setColour (Palette::displayDim);
            g.drawText ("-", left.removeFromTop (32.0f), juce::Justification::centredLeft);
        }
    }

    // Right: the send effects.
    g.setFont (Palette::label (10.0f));
    g.setColour (Palette::displayDim);
    g.drawText ("SEND EFFECTS", area.removeFromTop (16.0f), juce::Justification::centredLeft);

    const auto lineHeight = juce::jmin (20.0f, area.getHeight() / 3.0f);

    for (int slot = 0; slot < shownFx.size(); ++slot)
    {
        auto line = area.removeFromTop (lineHeight);
        auto bullet = line.removeFromLeft (14.0f);

        g.setColour (Palette::fxColour (slot));
        g.fillEllipse (bullet.withSizeKeepingCentre (6.0f, 6.0f));

        g.setFont (Palette::mono (11.5f, true));
        g.setColour (Palette::displayText);
        g.drawText (shownFx[slot], line, juce::Justification::centredLeft);
    }
}
