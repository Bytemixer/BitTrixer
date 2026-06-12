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

    void resized() override
    {
        auto b = content();
        auto right = b.removeFromRight (88);
        poles.setBounds (right.removeFromTop (22));
        right.removeFromTop (8);
        hpfOn.setBounds (right.removeFromTop (22));

        const int kw = b.getWidth() / 4;
        cutoff.setBounds (b.removeFromLeft (kw));
        res.setBounds    (b.removeFromLeft (kw));
        envAmt.setBounds (b.removeFromLeft (kw));
        hpfFreq.setBounds (b);
    }

private:
    LabeledKnob  cutoff, res, envAmt;
    ChoiceCombo  poles;
    SwitchToggle hpfOn;
    LabeledKnob  hpfFreq;
};
