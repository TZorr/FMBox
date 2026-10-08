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
    setInterceptsMouseClicks (false, false);
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

    juce::StringArray fx;
    for (int slot = 0; slot < FmParams::Fx::numSlots; ++slot)
        fx.add (fxLine (slot));

    if (sound != shownSound || hits != shownHits || fx != shownFx)
    {
        shownSound = sound;
        shownHits = hits;
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
        g.drawText (juce::MidiMessage::getMidiNoteName (spec.note, true, true, 3) + "  " + dot + "  note " + juce::String (spec.note)
                        + "  " + dot + "  vel " + juce::String (velocity),
                    left.removeFromTop (18.0f), juce::Justification::centredLeft);
    }
    else
    {
        g.setFont (Palette::mono (22.0f, true));
        g.setColour (Palette::displayDim);
        g.drawText ("-", left.removeFromTop (32.0f), juce::Justification::centredLeft);
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
