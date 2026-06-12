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
//  LfoPanel — one routable LFO: wave selector, rate and delay (fade-in).
// ============================================================================

class LfoPanel : public SectionPanel
{
public:
    LfoPanel (juce::AudioProcessorValueTreeState& s, int lfoIndex1Based)
        : SectionPanel ("LFO " + juce::String (lfoIndex1Based)),
          wave  (s, Params::lfoId (lfoIndex1Based, "wave")),
          glyph (s, Params::lfoId (lfoIndex1Based, "wave"), WaveGlyph::Set::Lfo),
          rate  (s, Params::lfoId (lfoIndex1Based, "rate"),  "RATE"),
          delay (s, Params::lfoId (lfoIndex1Based, "delay"), "DELAY")
    {
        addAndMakeVisible (wave);
        addAndMakeVisible (glyph);
        addAndMakeVisible (rate);
        addAndMakeVisible (delay);
    }

    void resized() override
    {
        auto b = content();
        auto top = b.removeFromTop (22);
        glyph.setBounds (top.removeFromRight (50));
        top.removeFromRight (4);
        wave.setBounds (top);
        b.removeFromTop (2);
        const int kw = b.getWidth() / 2;
        rate.setBounds (b.removeFromLeft (kw));
        delay.setBounds (b);
    }

private:
    ChoiceCombo wave;
    WaveGlyph   glyph;
    LabeledKnob rate, delay;
};
