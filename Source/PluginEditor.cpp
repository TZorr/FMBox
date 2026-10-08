//
//  PluginEditor.cpp
//  FMBox
//

#include "PluginEditor.h"
#include "UI/Palette.h"
#include "TextUtf8.h"

namespace
{
    constexpr int margin = 15, gap = 10;
    constexpr int columnWidth = 136, numColumns = FmParams::numSounds;
    constexpr int windowWidth = 2 * margin + numColumns * columnWidth + (numColumns - 1) * 6;
    constexpr int topRowHeight = 104;
    constexpr int padHeight = 58, knobRowHeight = 66, groupGap = 14;
    constexpr int columnHeight = 10 + padHeight + 8 + 6 * knobRowHeight + groupGap + 8;
    constexpr int sectionPadding = 8, titleHeight = 22;
    constexpr int fxHeight = titleHeight + 28 + 6 + knobRowHeight + 10;
    constexpr int footerHeight = 22;
    constexpr int windowHeight = margin + topRowHeight + gap + columnHeight + gap + fxHeight + 8 + footerHeight;

    /** Knob labels in FmParams::Snd::all order; the fourth is the sound's own. */
    juce::String knobLabel (int sound, int k)
    {
        static const char* const labels[] = { "Tune", "Decay", "FM", "", "Drive", "Click",
                                              "Level", "Pan", "Reverb", "Delay", "Mod", "Velo" };
        return k == 3 ? juce::String (FmParams::sounds[sound].toneLabel) : juce::String (labels[k]);
    }

    void setParameter (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        if (auto* parameter = state.getParameter (id))
        {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }
}

FMBoxEditor::FMBoxEditor (FMBoxProcessor& p)
    : AudioProcessorEditor (p), fmProcessor (p), display (p)
{
    using namespace FmParams;
    auto& state = fmProcessor.parameters;

    addAndMakeVisible (display);

    for (int s = 0; s < numSounds; ++s)
    {
        auto& column = columns[(size_t) s];
        column.pad = std::make_unique<PadButton> (fmProcessor, s);
        addAndMakeVisible (*column.pad);

        for (size_t k = 0; k < column.knobs.size(); ++k)
        {
            auto knob = std::make_unique<Knob> (knobLabel (s, (int) k), Snd::all[k] == Snd::pan);
            knob->attach (state, soundId (s, Snd::all[k]));
            addAndMakeVisible (*knob);
            column.knobs[k] = std::move (knob);
        }
    }

    for (int slot = 0; slot < Fx::numSlots; ++slot)
    {
        const auto& ids = Fx::ids[slot];
        auto& section = fx[(size_t) slot];
        const auto category = Fx::categories[slot];

        juce::StringArray names;
        for (int t = 0; t < EffectCatalog::numSubtypes (category); ++t)
            names.add (EffectCatalog::info (category, t).name);

        section.types.setLabels (names);
        section.types.onSelect = [this, slot] (int type)
        {
            setParameter (fmProcessor.parameters, Fx::ids[slot].type, (float) type);
            refreshFx();
        };

        section.a.attach (state, ids.a);
        section.b.attach (state, ids.b);
        section.level.attach (state, ids.level);

        // Set after attach(): the attachment installs its own text functions,
        // which only know "0..1". These print what the loaded effect means by
        // it - Hz, ms - and the delay's time as the note value it plays.
        section.a.textFromValueFunction = [this, slot] (double v)
        {
            const auto unit = slot == Fx::delay ? EffectCatalog::Unit::Division : fxInfo (slot).unitA;
            return EffectCatalog::format (unit, (float) v);
        };

        section.b.textFromValueFunction = [this, slot] (double v)
        {
            return EffectCatalog::format (fxInfo (slot).unitB, (float) v);
        };

        for (juce::Component* c : { (juce::Component*) &section.types, (juce::Component*) &section.a,
                                    (juce::Component*) &section.b, (juce::Component*) &section.level })
            addAndMakeVisible (c);
    }

    // The delay always follows the tempo, so its time knob steps through the
    // note values instead of sliding between them.
    fx[Fx::delay].a.setRange (0.0, 1.0, 1.0 / (Dsp::Sync::numDivisions - 1));

    masterKnob.attach (state, FmParams::master);
    addAndMakeVisible (masterKnob);

    refreshFx();
    display.refresh();

    setSize (windowWidth, windowHeight);
    startTimerHz (30);
}

FMBoxEditor::~FMBoxEditor()
{
    stopTimer();
}

const EffectCatalog::Info& FMBoxEditor::fxInfo (int slot) const
{
    const auto category = FmParams::Fx::categories[slot];
    const auto type = (int) std::lround (fmProcessor.parameters.getRawParameterValue (FmParams::Fx::ids[slot].type)->load());
    return EffectCatalog::info (category, EffectCatalog::clampSubtype (category, type));
}

void FMBoxEditor::refreshFx()
{
    for (int slot = 0; slot < FmParams::Fx::numSlots; ++slot)
    {
        auto& section = fx[(size_t) slot];
        const auto category = FmParams::Fx::categories[slot];
        const auto type = EffectCatalog::clampSubtype (
            category, (int) std::lround (fmProcessor.parameters.getRawParameterValue (FmParams::Fx::ids[slot].type)->load()));

        if (type == section.shownType)
            continue;

        // A type change - from a button, automation or a restored session -
        // lights the button, renames the two effect knobs and reprints their
        // values in the new units.
        section.shownType = type;
        section.types.setSelected (type);

        const auto& info = fxInfo (slot);
        section.a.setLabel (info.labelA);
        section.b.setLabel (info.labelB);
    }
}

void FMBoxEditor::timerCallback()
{
    fmProcessor.applyLearnedNote();

    for (auto& column : columns)
        column.pad->updateFlash();

    refreshFx();
    display.refresh();
}

void FMBoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (Palette::body);

    // Wordmark, as on Kitbox and Rackbox: orange then ink.
    {
        auto area = wordmarkArea.toFloat();
        const auto font = juce::Font (juce::FontOptions (30.0f, juce::Font::bold)).withExtraKerningFactor (0.16f);
        g.setFont (font);

        const auto fmWidth = juce::GlyphArrangement::getStringWidth (font, "FM");
        auto nameRow = area.removeFromTop (58.0f).withTrimmedTop (14.0f);

        g.setColour (Palette::accent);
        g.drawText ("FM", nameRow, juce::Justification::centredLeft);
        g.setColour (Palette::ink);
        g.drawText ("BOX", nameRow.withTrimmedLeft (fmWidth + 1.0f), juce::Justification::centredLeft);

        g.setFont (Palette::label (9.5f));
        g.setColour (Palette::inkDim);
        g.drawText ("8-SOUND FM DRUMS", area.removeFromTop (14.0f), juce::Justification::centredLeft);
    }

    const auto drawSection = [&] (juce::Rectangle<int> bounds, const juce::String& title, juce::Colour dot)
    {
        g.setColour (Palette::section);
        g.fillRoundedRectangle (bounds.toFloat(), 6.0f);

        auto titleRow = bounds.reduced (sectionPadding + 2, 0).removeFromTop (titleHeight).toFloat().withTrimmedTop (3.0f);

        if (! dot.isTransparent())
        {
            g.setColour (dot);
            g.fillEllipse (titleRow.removeFromLeft (8.0f).withSizeKeepingCentre (6.0f, 6.0f));
            titleRow.removeFromLeft (6.0f);
        }

        g.setColour (Palette::ink);
        g.setFont (Palette::label (10.0f));
        g.drawText (title.toUpperCase(), titleRow, juce::Justification::centredLeft);
    };

    drawSection (outputArea, "Output", juce::Colours::transparentBlack);

    // A recessed column per sound, with a hairline between the sound's own
    // knobs and its mix.
    for (int s = 0; s < FmParams::numSounds; ++s)
    {
        const auto area = columnAreas[(size_t) s];
        g.setColour (Palette::section);
        g.fillRoundedRectangle (area.toFloat(), 6.0f);

        const auto y = (float) (area.getY() + 10 + 3 * knobRowHeight + groupGap / 2);
        g.setColour (Palette::sectionLine);
        g.drawHorizontalLine ((int) y, (float) area.getX() + 12.0f, (float) area.getRight() - 12.0f);
    }

    for (int slot = 0; slot < FmParams::Fx::numSlots; ++slot)
        drawSection (fxAreas[(size_t) slot], FmParams::Fx::titles[slot], Palette::fxColour (slot));

    g.setColour (Palette::inkDim);
    g.setFont (Palette::label (9.0f));
    g.drawText (utf8 ("FMBOX " JucePlugin_VersionString "   \xc2\xb7   CLICK A PAD TO SELECT AND HEAR IT   \xc2\xb7   LEARN: PICK A PAD, PRESS LEARN, PLAY A KEY"
                      "   \xc2\xb7   SENDS ARE POST-LEVEL, PRE-PAN   \xc2\xb7   CLOSED HAT CHOKES OPEN HAT"),
                footerArea, juce::Justification::centredLeft);
}

void FMBoxEditor::resized()
{
    auto area = getLocalBounds().reduced (margin, 0);
    area.removeFromTop (margin);

    // Top row.
    {
        auto top = area.removeFromTop (topRowHeight);
        wordmarkArea = top.removeFromLeft (190);
        outputArea = top.removeFromRight (120);
        top.removeFromRight (gap);
        display.setBounds (top);

        auto knobArea = outputArea.reduced (sectionPadding, 0);
        knobArea.removeFromTop (titleHeight);
        masterKnob.setBounds (knobArea.withSizeKeepingCentre (76, knobArea.getHeight() - 4));
    }

    area.removeFromTop (gap);

    // The send effects, right under the display.
    {
        auto row = area.removeFromTop (fxHeight);
        const auto width = (row.getWidth() - 2 * gap) / 3;

        for (int slot = 0; slot < FmParams::Fx::numSlots; ++slot)
        {
            auto section = slot < 2 ? row.removeFromLeft (width) : row;
            row.removeFromLeft (gap);
            fxAreas[(size_t) slot] = section;

            auto inner = section.reduced (sectionPadding + 2, 0);
            inner.removeFromTop (titleHeight);

            auto& f = fx[(size_t) slot];
            f.types.setBounds (inner.removeFromTop (28));
            inner.removeFromTop (6);

            auto knobs = inner.removeFromTop (knobRowHeight);
            const auto cell = knobs.getWidth() / 3;
            f.a.setBounds (knobs.removeFromLeft (cell));
            f.b.setBounds (knobs.removeFromLeft (cell));
            f.level.setBounds (knobs);
        }
    }

    area.removeFromTop (gap);

    // The sound columns: the sound's knobs, the sends, Level / Pan and the
    // pad at the bottom, where a hand reaches for it first.
    {
        // Pairs of indices into FmParams::Snd::all, top to bottom.
        static constexpr int rows[6][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 },      // Tune Decay, FM Tone, Drive Click
                                            { 8, 9 }, { 10, 11 }, { 6, 7 } };  // Reverb Delay, Mod Velo, Level Pan

        auto row = area.removeFromTop (columnHeight);

        for (int s = 0; s < FmParams::numSounds; ++s)
        {
            auto column = row.removeFromLeft (columnWidth);
            row.removeFromLeft (6);
            columnAreas[(size_t) s] = column;

            auto inner = column.reduced (9, 0);
            inner.removeFromTop (10);

            auto& c = columns[(size_t) s];

            for (int r = 0; r < 6; ++r)
            {
                if (r == 3)
                    inner.removeFromTop (groupGap);

                auto line = inner.removeFromTop (knobRowHeight);
                const auto half = line.getWidth() / 2;
                c.knobs[(size_t) rows[r][0]]->setBounds (line.removeFromLeft (half));
                c.knobs[(size_t) rows[r][1]]->setBounds (line);
            }

            inner.removeFromTop (8);
            c.pad->setBounds (inner.removeFromTop (padHeight));
        }
    }

    area.removeFromTop (8);
    footerArea = area.removeFromTop (footerHeight);
}
