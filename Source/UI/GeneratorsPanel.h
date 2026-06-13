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
        for (int i = 1; i <= Params::kNumOscs; ++i)
        {
            auto* o = oscs.add (new OscBand (s, i));
            addAndMakeVisible (o);
        }
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

        // noise is the shorter band; the three oscillators share the rest.
        // noiseH must still fit a full control block so its knobs match the
        // oscillator knobs exactly (the knob region is fixed, see LabeledKnob).
        const int noiseH = 100;
        const int divGap = 6;
        const int oscH = (b.getHeight() - noiseH - divGap * Params::kNumOscs)
                       / Params::kNumOscs;

        for (int i = 0; i < oscs.size(); ++i)
        {
            oscs[i]->setBounds (b.removeFromTop (oscH));
            b.removeFromTop (divGap);
            dividers.push_back ((float) b.getY() - divGap * 0.5f);
        }
        noise->setBounds (b.removeFromTop (noiseH));
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
            glyph.setBounds (top.removeFromRight (44));
            top.removeFromRight (3);
            if (syncSwitch != nullptr)
                syncSwitch->setBounds (top.removeFromRight (66));
            else if (masterLabel.isVisible())
                masterLabel.setBounds (top.removeFromRight (66));
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

    juce::OwnedArray<OscBand> oscs;
    std::unique_ptr<NoiseBand> noise;
    std::vector<float> dividers;
};
