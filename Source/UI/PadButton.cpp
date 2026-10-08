//
//  PadButton.cpp
//  FMBox
//

#include "PadButton.h"
#include "Palette.h"
#include "PluginProcessor.h"

PadButton::PadButton (FMBoxProcessor& processorToUse, int soundIndex)
    : processor (processorToUse), sound (soundIndex)
{
    setRepaintsOnMouseActivity (true);
    lastHitCount = processor.getEngine().getHitCount (sound);
    shownNote = processor.getSoundNote (sound);
    selected = processor.getSelectedSound() == sound;
}

void PadButton::updateFlash()
{
    const auto note = processor.getSoundNote (sound);
    const auto isSelected = processor.getSelectedSound() == sound;

    if (note != shownNote || isSelected != selected)
    {
        shownNote = note;
        selected = isSelected;
        repaint();
    }

    const auto hits = processor.getEngine().getHitCount (sound);

    if (hits != lastHitCount)
    {
        lastHitCount = hits;
        flash = 0.35f + 0.65f * processor.getEngine().getLastVelocity (sound);
        repaint();
    }
    else if (flash > 0.0f)
    {
        flash = flash < 0.02f ? 0.0f : flash * 0.78f;
        repaint();
    }
}

void PadButton::mouseDown (const juce::MouseEvent& event)
{
    const auto height = juce::jmax (1, getHeight());
    const auto velocity = 1.0f - 0.67f * juce::jlimit (0.0f, 1.0f, (float) event.y / (float) height);
    processor.setSelectedSound (sound);

    // While Learn waits, a click on another pad hands the wait over to it.
    if (processor.getLearnSound() >= 0)
        processor.startLearn (sound);

    processor.playSound (sound, velocity);
    updateFlash();
}

void PadButton::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto colour = Palette::soundColour (sound);
    const auto& spec = FmParams::sounds[sound];

    auto fill = isMouseOver() ? Palette::padHover : Palette::pad;
    fill = fill.interpolatedWith (colour, flash * 0.85f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 6.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, 6.0f);
        g.reduceClipRegion (clip);
        g.setColour (colour);
        g.fillRect (bounds.withHeight (4.0f));
    }

    if (selected)
    {
        g.setColour (Palette::accent);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 2.0f);
    }

    auto text = bounds.reduced (10.0f, 8.0f);
    text.removeFromTop (4.0f);

    const auto dark = flash > 0.45f;

    g.setFont (Palette::label (12.0f));
    g.setColour (dark ? Palette::pad : Palette::padText);
    g.drawFittedText (juce::String (spec.name).toUpperCase(), text.removeFromTop (18.0f).toNearestInt(),
                      juce::Justification::topLeft, 1, 0.8f);

    g.setFont (Palette::mono (10.5f));
    g.setColour (dark ? Palette::pad : Palette::padTextDim);
    g.drawText (FmParams::noteName (shownNote) + "  " + juce::String (shownNote), text, juce::Justification::bottomLeft);
}
