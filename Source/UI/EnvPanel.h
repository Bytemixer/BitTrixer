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
//  EnvPanel — one ADSR section as DeepMind-style vertical faders, plus the
//  curve-shape pot and the invert switch. Used twice (filter env, amp env).
// ============================================================================

class EnvPanel : public SectionPanel
{
public:
    EnvPanel (juce::AudioProcessorValueTreeState& s, const juce::String& titleText,
              const char* attackId, const char* decayId, const char* sustainId,
              const char* releaseId, const char* curveId, const char* invertId)
        : SectionPanel (titleText),
          a (s, attackId,  "A"),
          d (s, decayId,   "D"),
          su (s, sustainId, "S"),
          r (s, releaseId, "R"),
          curve  (s, curveId, "CURVE"),
          invert (s, invertId, "INV")
    {
        addAndMakeVisible (a);
        addAndMakeVisible (d);
        addAndMakeVisible (su);
        addAndMakeVisible (r);
        addAndMakeVisible (curve);
        addAndMakeVisible (invert);
    }

    void resized() override
    {
        auto b = content();
        auto right = b.removeFromRight (92);
        curve.setBounds (right.removeFromTop (juce::jmin (76, right.getHeight() - 24)));
        invert.setBounds (right.removeFromTop (22).withTrimmedLeft (8));

        const int fw = b.getWidth() / 4;
        a.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
        d.setBounds  (b.removeFromLeft (fw).reduced (4, 0));
        su.setBounds (b.removeFromLeft (fw).reduced (4, 0));
        r.setBounds  (b.reduced (4, 0));
    }

private:
    VFader a, d, su, r;
    LabeledKnob  curve;
    SwitchToggle invert;
};
