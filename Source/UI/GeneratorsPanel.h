/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#pragma once

#include "PanelCommon.h"
#include "WaveGlyph.h"
#include "FmAlgoGlyph.h"
#include "../Params.h"

// ============================================================================
//  GeneratorsPanel — all sound sources in one panel: OSC 1/2/3 + NOISE,
//  each as a labelled band separated by thin dividers. OSC 1 is the sync
//  MASTER; OSC 2/3 each carry their own SYNC switch (hard-sync to OSC 1).
// ============================================================================

class GeneratorsPanel : public SectionPanel
{
public:
    explicit GeneratorsPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Sound Generators")
    {
        // OSC 3's slot is the 4-operator FM voice, shown as its own (taller)
        // band before Noise; only OSC 1 and OSC 2 are plain oscillators now.
        for (int i = 1; i <= Params::kNumOscs - 1; ++i)
        {
            auto* o = oscs.add (new OscBand (s, i));
            addAndMakeVisible (o);
        }
        fm = std::make_unique<FmBand> (s);
        addAndMakeVisible (fm.get());
        noise = std::make_unique<NoiseBand> (s);
        addAndMakeVisible (noise.get());
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        g.setColour (RetroColors::panelEdge.withAlpha (0.7f));
        for (float y : dividers)
            g.drawLine (getLocalBounds().toFloat().getX() + 10.0f, y,
                        getLocalBounds().toFloat().getRight() - 10.0f, y, 1.0f);
    }

    void resized() override
    {
        auto b = content();
        dividers.clear();

        // OSC 1/2 and Noise are compact (one control block each); the taller FM
        // band sits between OSC 2 and Noise and takes whatever height is left.
        const int compactH = 96;
        const int divGap = 6;
        const int fmH = juce::jmax (150, b.getHeight() - compactH * 3 - divGap * 3);

        auto place = [&] (juce::Component* c, int h)
        {
            c->setBounds (b.removeFromTop (h));
            b.removeFromTop (divGap);
            dividers.push_back ((float) b.getY() - divGap * 0.5f);
        };
        if (oscs.size() > 0) place (oscs[0], compactH);
        if (oscs.size() > 1) place (oscs[1], compactH);
        place (fm.get(), fmH);
        noise->setBounds (b.removeFromTop (compactH));   // last band, no trailing divider
    }

private:
    // ---- a labelled tag strip used at the left of each band ----
    struct BandTag : juce::Component
    {
        BandTag (juce::String t, juce::Colour c) : text (std::move (t)), colour (c) {}
        void paint (juce::Graphics& g) override
        {
            g.setColour (colour);
            g.fillRoundedRectangle (1.0f, 2.0f, 3.0f, (float) getHeight() - 4.0f, 1.5f);
            g.setColour (RetroColors::panelTitle);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            g.drawText (text, getLocalBounds().withTrimmedLeft (8),
                        juce::Justification::centredLeft);
        }
        juce::String text;
        juce::Colour colour;
    };

    // ---- one oscillator: enable, sync (or MASTER tag), wave + glyph, knobs ----
    struct OscBand : juce::Component
    {
        OscBand (juce::AudioProcessorValueTreeState& s, int idx)
            : index (idx),
              tag ("OSC " + juce::String (idx), RetroColors::accent),
              onSwitch (s, Params::oscId (idx, "on"), ""),
              wave  (s, Params::oscId (idx, "wave")),
              glyph (s, Params::oscId (idx, "wave"), WaveGlyph::Set::Osc),
              pitch (s, Params::oscId (idx, "pitch"), "PITCH"),
              fine  (s, Params::oscId (idx, "fine"),  "FINE"),
              pwm   (s, Params::oscId (idx, "pwm"),   "PWM"),
              fold  (s, Params::oscId (idx, "fold"),  "FOLD"),
              level (s, Params::oscId (idx, "level"), "LEVEL")
        {
            addAndMakeVisible (tag);
            addAndMakeVisible (onSwitch);
            addAndMakeVisible (wave);
            addAndMakeVisible (glyph);
            for (auto* k : { &pitch, &fine, &pwm, &fold, &level })
                addAndMakeVisible (k);

            if (idx == 1)
            {
                masterLabel.setText ("MASTER", juce::dontSendNotification);
                masterLabel.setJustificationType (juce::Justification::centred);
                masterLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
                masterLabel.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
                addAndMakeVisible (masterLabel);
            }
            else
            {
                syncSwitch = std::make_unique<SwitchToggle> (s, Params::oscId (idx, "sync"), "SYNC");
                addAndMakeVisible (syncSwitch.get());
            }
        }

        void resized() override
        {
            // centre a 90px control block in the band (no floating knobs);
            // 20 top row + 70 knob row fits the full fixed knob region
            constexpr int blockH = 90;
            auto full = getLocalBounds().reduced (2, 2);
            auto b = full.withTrimmedTop (juce::jmax (0, (full.getHeight() - blockH) / 2))
                         .withHeight (juce::jmin (full.getHeight(), blockH));

            auto top = b.removeFromTop (20);
            tag.setBounds (top.removeFromLeft (48));
            onSwitch.setBounds (top.removeFromLeft (32));
            // far right: SYNC switch (or MASTER tag); the wave-shape glyph
            // sits next to the wave selector, not between selector and sync
            if (syncSwitch != nullptr)
                syncSwitch->setBounds (top.removeFromRight (66));
            else if (masterLabel.isVisible())
                masterLabel.setBounds (top.removeFromRight (66));
            top.removeFromRight (4);
            glyph.setBounds (top.removeFromRight (40));
            top.removeFromLeft (3);
            wave.setBounds (top);

            b.removeFromTop (2);
            const int kw = b.getWidth() / 5;       // shared 5-column knob grid
            pitch.setBounds (b.removeFromLeft (kw));
            fine.setBounds  (b.removeFromLeft (kw));
            pwm.setBounds   (b.removeFromLeft (kw));
            fold.setBounds  (b.removeFromLeft (kw));
            level.setBounds (b.removeFromLeft (kw));
        }

        int index;
        BandTag tag;
        SwitchToggle onSwitch;
        ChoiceCombo  wave;
        WaveGlyph    glyph;
        LabeledKnob  pitch, fine, pwm, fold, level;
        std::unique_ptr<SwitchToggle> syncSwitch;
        juce::Label masterLabel;
    };

    // ---- the noise generator band ----
    struct NoiseBand : juce::Component
    {
        explicit NoiseBand (juce::AudioProcessorValueTreeState& s)
            : tag ("NOISE", juce::Colour (0xffb07cc6)),
              onSwitch (s, Params::id::noiseOn, ""),
              type (s, Params::id::noiseType),
              color (s, Params::id::noiseColor, "COLOR"),
              level (s, Params::id::noiseLevel, "LEVEL")
        {
            addAndMakeVisible (tag);
            addAndMakeVisible (onSwitch);
            addAndMakeVisible (type);
            addAndMakeVisible (color);
            addAndMakeVisible (level);
        }

        void resized() override
        {
            // same centred 90px block as the oscillator bands, so the noise
            // knobs are exactly the oscillator knob size
            constexpr int blockH = 90;
            auto full = getLocalBounds().reduced (2, 2);
            auto b = full.withTrimmedTop (juce::jmax (0, (full.getHeight() - blockH) / 2))
                         .withHeight (juce::jmin (full.getHeight(), blockH));

            auto top = b.removeFromTop (20);
            tag.setBounds (top.removeFromLeft (48));
            onSwitch.setBounds (top.removeFromLeft (32));
            top.removeFromLeft (3);
            type.setBounds (top);

            b.removeFromTop (2);
            // same 5-column grid as the oscillators: COLOR + LEVEL occupy the
            // first two cells so every generator knob is the same size and
            // column-aligned. Cells 3-5 stay open (room for the routing tap).
            const int kw = b.getWidth() / 5;
            color.setBounds (b.removeFromLeft (kw));
            level.setBounds (b.removeFromLeft (kw));
        }

        BandTag tag;
        SwitchToggle onSwitch;
        ChoiceCombo  type;
        LabeledKnob  color, level;
    };

    // ---- the 4-operator FM voice band: header (tag + enable + algorithm
    //      selector + algorithm glyph), then a 4x3 knob grid (transpose/output/
    //      feedback row, then the operator ratio and level rows) ----
    struct FmBand : juce::Component
    {
        explicit FmBand (juce::AudioProcessorValueTreeState& s)
            : tag ("FM", RetroColors::accent),
              onSwitch (s, Params::oscId (3, "on"), ""),
              algo  (s, Params::id::fmAlgo),
              glyph (s),
              pitch (s, Params::oscId (3, "pitch"), "PITCH"),
              fine  (s, Params::oscId (3, "fine"),  "FINE"),
              outLevel (s, Params::oscId (3, "level"), "LEVEL"),
              feedback (s, Params::id::fmFeedback,      "FDBK")
        {
            addAndMakeVisible (tag);
            addAndMakeVisible (onSwitch);
            addAndMakeVisible (algo);
            addAndMakeVisible (glyph);
            for (auto* k : { &pitch, &fine, &outLevel, &feedback })
                addAndMakeVisible (k);

            static const char* opNames[4] = { "OP1", "OP2", "OP3", "OP4" };
            for (int i = 0; i < 4; ++i)
            {
                ops[i].ratio = std::make_unique<LabeledKnob> (s, Params::fmOpId (i + 1, "ratio"), opNames[i]);
                ops[i].level = std::make_unique<LabeledKnob> (s, Params::fmOpId (i + 1, "level"), "LVL");
                addAndMakeVisible (*ops[i].ratio);
                addAndMakeVisible (*ops[i].level);
            }
        }

        void resized() override
        {
            auto full = getLocalBounds().reduced (3, 2);

            // header: tag + enable + (shorter) algorithm selector + glyph square
            auto head = full.removeFromTop (40);
            tag.setBounds (head.removeFromLeft (40));
            onSwitch.setBounds (head.removeFromLeft (34).withSizeKeepingCentre (32, 18));
            head.removeFromLeft (4);
            auto glyphBox = head.removeFromRight (40);
            glyph.setBounds (glyphBox.withSizeKeepingCentre (38, 38));
            head.removeFromRight (5);
            algo.setBounds (head.withSizeKeepingCentre (head.getWidth(), 22));

            full.removeFromTop (2);

            // 4-column x 3-row knob grid
            const int rowH = full.getHeight() / 3;
            auto row1 = full.removeFromTop (rowH);   // PITCH FINE LEVEL FDBK
            auto row2 = full.removeFromTop (rowH);   // OP1..OP4 ratio
            auto row3 = full;                        // OP1..OP4 level

            auto place4 = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> cs)
            {
                const int cw = r.getWidth() / 4;
                int i = 0;
                for (auto* c : cs) { c->setBounds (r.removeFromLeft (i == 3 ? r.getWidth() : cw)); ++i; }
            };
            place4 (row1, { &pitch, &fine, &outLevel, &feedback });
            place4 (row2, { ops[0].ratio.get(), ops[1].ratio.get(), ops[2].ratio.get(), ops[3].ratio.get() });
            place4 (row3, { ops[0].level.get(), ops[1].level.get(), ops[2].level.get(), ops[3].level.get() });
        }

        struct OpStrip { std::unique_ptr<LabeledKnob> ratio, level; };

        BandTag tag;
        SwitchToggle onSwitch;
        ChoiceCombo  algo;
        FmAlgoGlyph  glyph;
        LabeledKnob  pitch, fine, outLevel, feedback;
        OpStrip      ops[4];
    };

    juce::OwnedArray<OscBand> oscs;
    std::unique_ptr<FmBand> fm;
    std::unique_ptr<NoiseBand> noise;
    std::vector<float> dividers;
};
