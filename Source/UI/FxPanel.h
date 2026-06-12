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
#include "../Params.h"

// ============================================================================
//  FxPanel — the integrated effects strip:
//   * BITCRUSH  (per voice, between mixer and VCF — the filter smooths it)
//   * PHASER    (per trigger-instance, after the voice sum)
//   * FLANGER   (per trigger-instance, after the phaser)
//  All retrigger with the sound, so they are part of the SFX itself.
// ============================================================================

class FxPanel : public SectionPanel
{
public:
    explicit FxPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("FX (crush: pre-filter / phaser+flanger: per sound)"),
          crushOn (s, Params::id::crushOn, "CRUSH"),
          crushBits (s, Params::id::crushBits, "BITS"),
          crushDown (s, Params::id::crushDown, "RATE DIV"),
          phaseOn (s, Params::id::phaseOn, "PHASER"),
          phaseRate (s, Params::id::phaseRate, "RATE"),
          phaseDepth (s, Params::id::phaseDepth, "DEPTH"),
          phaseFb (s, Params::id::phaseFb, "FDBK"),
          flangeOn (s, Params::id::flangeOn, "FLANGER"),
          flangeRate (s, Params::id::flangeRate, "RATE"),
          flangeDepth (s, Params::id::flangeDepth, "DEPTH"),
          flangeFb (s, Params::id::flangeFb, "FDBK")
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &crushOn, &crushBits, &crushDown,
                 &phaseOn, &phaseRate, &phaseDepth, &phaseFb,
                 &flangeOn, &flangeRate, &flangeDepth, &flangeFb })
            addAndMakeVisible (c);
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        g.setColour (RetroColors::panelEdge);
        for (float x : { sep1, sep2 })
            if (x > 0.0f)
                g.drawLine (x, (float) content().getY() + 4.0f,
                            x, (float) content().getBottom() - 4.0f, 1.0f);
    }

    void resized() override
    {
        auto b = content();
        const int third = b.getWidth() / 3;

        auto lay = [] (juce::Rectangle<int> area, SwitchToggle& sw,
                       std::initializer_list<LabeledKnob*> knobs)
        {
            sw.setBounds (area.removeFromLeft (92).withSizeKeepingCentre (92, 22));
            const int kw = area.getWidth() / (int) knobs.size();
            for (auto* k : knobs)
                k->setBounds (area.removeFromLeft (kw));
        };

        auto a1 = b.removeFromLeft (third);
        sep1 = (float) b.getX();
        auto a2 = b.removeFromLeft (third);
        sep2 = (float) b.getX();

        lay (a1.reduced (2, 0), crushOn, { &crushBits, &crushDown });
        lay (a2.reduced (6, 0), phaseOn, { &phaseRate, &phaseDepth, &phaseFb });
        lay (b.reduced (6, 0), flangeOn, { &flangeRate, &flangeDepth, &flangeFb });
    }

private:
    SwitchToggle crushOn;
    LabeledKnob  crushBits, crushDown;
    SwitchToggle phaseOn;
    LabeledKnob  phaseRate, phaseDepth, phaseFb;
    SwitchToggle flangeOn;
    LabeledKnob  flangeRate, flangeDepth, flangeFb;

    float sep1 = 0.0f, sep2 = 0.0f;
};
