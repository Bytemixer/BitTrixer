/*  This file is part of the BitTrixer audio plugin.
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
//  FilterPanel — the VCF: cutoff/resonance pots, 2/4-pole selector,
//  dedicated filter-envelope amount, and the switchable HPF.
// ============================================================================

class FilterPanel : public SectionPanel
{
public:
    explicit FilterPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("VCF Lowpass + HPF"),
          cutoff (s, Params::id::lpfCutoff, "CUTOFF"),
          res    (s, Params::id::lpfRes,    "RESO"),
          envAmt (s, Params::id::lpfEnv,    "ENV AMT"),
          poles  (s, Params::id::lpfPoles),
          hpfOn  (s, Params::id::hpfOn, "HPF"),
          hpfFreq (s, Params::id::hpfCutoff, "HPF FREQ")
    {
        addAndMakeVisible (cutoff);
        addAndMakeVisible (res);
        addAndMakeVisible (envAmt);
        addAndMakeVisible (poles);
        addAndMakeVisible (hpfOn);
        addAndMakeVisible (hpfFreq);
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);
        // divider between the LPF group and the HPF group
        if (hpfDivX > 0.0f)
        {
            g.setColour (RetroColors::panelEdge.withAlpha (0.8f));
            g.drawLine (hpfDivX, (float) content().getY() + 2.0f,
                        hpfDivX, (float) content().getBottom() - 2.0f, 1.0f);
        }
        // sub-section labels
        g.setColour (RetroColors::textDim);
        g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
        g.drawText ("LOW-PASS", lpfLabel, juce::Justification::centredLeft);
        g.drawText ("HIGH-PASS", hpfLabel, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = content();

        // ---- HPF group on the right (switch + cutoff), behind a divider ----
        auto hpf = b.removeFromRight (96);
        hpfDivX = (float) hpf.getX() - 4.0f;
        hpfLabel = hpf.removeFromTop (12);
        hpfOn.setBounds (hpf.removeFromTop (24).withTrimmedLeft (6));
        hpfFreq.setBounds (hpf);

        b.removeFromRight (8);   // gap for the divider

        // ---- LPF group on the left: label, then knobs + slope selector ----
        lpfLabel = b.removeFromTop (12);
        // 2/4-pole slope sits with the LPF controls it actually affects
        auto slope = b.removeFromRight (78);
        slope.removeFromTop (4);
        poles.setBounds (slope.removeFromTop (24));

        const int kw = b.getWidth() / 3;
        cutoff.setBounds (b.removeFromLeft (kw));
        res.setBounds    (b.removeFromLeft (kw));
        envAmt.setBounds (b.removeFromLeft (kw));
    }

private:
    LabeledKnob  cutoff, res, envAmt;
    ChoiceCombo  poles;
    SwitchToggle hpfOn;
    LabeledKnob  hpfFreq;
    juce::Rectangle<int> lpfLabel, hpfLabel;
    float hpfDivX = 0.0f;
};
