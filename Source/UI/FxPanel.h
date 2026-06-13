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
//  FxPanel — the FX chain strip: seven named effects in a row, each an enable
//  switch + its knobs (RingMod / Tremolo also carry a waveform selector).
//   * CRUSH    : per voice, pre-filter (the filter smooths the grit)
//   * the rest : per trigger-instance, after the voice sum, in series
//  All retrigger with the sound, so they are part of the SFX itself.
//  (Drag-to-reorder + moving Crush into the chain land in a later pass.)
// ============================================================================

class FxPanel : public SectionPanel
{
public:
    explicit FxPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("FX chain"),
          crushOn (s, Params::id::crushOn, "CRUSH"),
          crushBits (s, Params::id::crushBits, "BITS", false),
          crushDown (s, Params::id::crushDown, "DIV", false),
          phaseOn (s, Params::id::phaseOn, "PHASER"),
          phaseRate (s, Params::id::phaseRate, "RATE", false),
          phaseDepth (s, Params::id::phaseDepth, "DEPTH", false),
          phaseFb (s, Params::id::phaseFb, "FDBK", false),
          flangeOn (s, Params::id::flangeOn, "FLANGER"),
          flangeRate (s, Params::id::flangeRate, "RATE", false),
          flangeDepth (s, Params::id::flangeDepth, "DEPTH", false),
          flangeFb (s, Params::id::flangeFb, "FDBK", false),
          ringOn (s, Params::id::ringOn, "RING"),
          ringFreq (s, Params::id::ringFreq, "FREQ", false),
          ringMix (s, Params::id::ringMix, "WET", false),
          ringWave (s, Params::id::ringWave),
          tremOn (s, Params::id::tremOn, "TREM"),
          tremRate (s, Params::id::tremRate, "SPEED", false),
          tremDepth (s, Params::id::tremDepth, "DEPTH", false),
          tremWave (s, Params::id::tremWave),
          formOn (s, Params::id::formOn, "FORMANT"),
          formVowel (s, Params::id::formVowel, "VOWEL", false),
          formReso (s, Params::id::formReso, "RESO", false),
          formMix (s, Params::id::formMix, "WET", false),
          delayOn (s, Params::id::delayOn, "DELAY"),
          delayTime (s, Params::id::delayTime, "TIME", false),
          delayFb (s, Params::id::delayFb, "FDBK", false),
          delayMix (s, Params::id::delayMix, "WET", false)
    {
        for (auto* c : std::initializer_list<juce::Component*> {
                 &crushOn, &crushBits, &crushDown,
                 &phaseOn, &phaseRate, &phaseDepth, &phaseFb,
                 &flangeOn, &flangeRate, &flangeDepth, &flangeFb,
                 &ringOn, &ringFreq, &ringMix, &ringWave,
                 &tremOn, &tremRate, &tremDepth, &tremWave,
                 &formOn, &formVowel, &formReso, &formMix,
                 &delayOn, &delayTime, &delayFb, &delayMix })
            addAndMakeVisible (c);
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        g.setColour (RetroColors::panelEdge);
        for (float x : sepX)
            if (x > 0.0f)
                g.drawLine (x, (float) content().getY() + 2.0f,
                            x, (float) content().getBottom() - 2.0f, 1.0f);
    }

    void resized() override
    {
        auto b = content();
        const int colW = b.getWidth() / 7;

        int sepIdx = 0;
        auto nextCol = [&] () -> juce::Rectangle<int>
        {
            auto c = b.removeFromLeft (colW);
            if (sepIdx < 6) sepX[(size_t) sepIdx++] = (float) b.getX();
            return c.reduced (3, 1);
        };

        auto knobRow = [] (juce::Rectangle<int> row, std::initializer_list<LabeledKnob*> knobs)
        {
            const int kw = row.getWidth() / (int) knobs.size();
            for (auto* k : knobs) k->setBounds (row.removeFromLeft (kw));
        };

        // a column whose enable switch shares its top row with a wave selector
        auto headRowWithCombo = [] (juce::Rectangle<int>& col, SwitchToggle& sw, ChoiceCombo& combo)
        {
            auto top = col.removeFromTop (20);
            combo.setBounds (top.removeFromRight (62));
            sw.setBounds (top);
            col.removeFromTop (2);
        };

        { auto c = nextCol(); crushOn.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
          knobRow (c, { &crushBits, &crushDown }); }

        { auto c = nextCol(); phaseOn.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
          knobRow (c, { &phaseRate, &phaseDepth, &phaseFb }); }

        { auto c = nextCol(); flangeOn.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
          knobRow (c, { &flangeRate, &flangeDepth, &flangeFb }); }

        { auto c = nextCol(); headRowWithCombo (c, ringOn, ringWave);
          knobRow (c, { &ringFreq, &ringMix }); }

        { auto c = nextCol(); headRowWithCombo (c, tremOn, tremWave);
          knobRow (c, { &tremRate, &tremDepth }); }

        { auto c = nextCol(); formOn.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
          knobRow (c, { &formVowel, &formReso, &formMix }); }

        { auto c = b.reduced (3, 1); delayOn.setBounds (c.removeFromTop (20)); c.removeFromTop (2);
          knobRow (c, { &delayTime, &delayFb, &delayMix }); }
    }

private:
    SwitchToggle crushOn;   LabeledKnob crushBits, crushDown;
    SwitchToggle phaseOn;   LabeledKnob phaseRate, phaseDepth, phaseFb;
    SwitchToggle flangeOn;  LabeledKnob flangeRate, flangeDepth, flangeFb;
    SwitchToggle ringOn;    LabeledKnob ringFreq, ringMix;     ChoiceCombo ringWave;
    SwitchToggle tremOn;    LabeledKnob tremRate, tremDepth;   ChoiceCombo tremWave;
    SwitchToggle formOn;    LabeledKnob formVowel, formReso, formMix;
    SwitchToggle delayOn;   LabeledKnob delayTime, delayFb, delayMix;

    float sepX[6] { 0, 0, 0, 0, 0, 0 };
};
