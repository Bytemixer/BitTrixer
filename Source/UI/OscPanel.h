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
//  OscPanel — one VCO strip: on/off switch, wave selector, and the
//  pitch / fine / PWM / fold / level pots.
// ============================================================================

class OscPanel : public SectionPanel
{
public:
    OscPanel (juce::AudioProcessorValueTreeState& s, int oscIndex1Based)
        : SectionPanel ("OSC " + juce::String (oscIndex1Based)),
          onSwitch (s, Params::oscId (oscIndex1Based, "on"), ""),
          wave  (s, Params::oscId (oscIndex1Based, "wave")),
          pitch (s, Params::oscId (oscIndex1Based, "pitch"), "PITCH"),
          fine  (s, Params::oscId (oscIndex1Based, "fine"),  "FINE"),
          pwm   (s, Params::oscId (oscIndex1Based, "pwm"),   "PWM"),
          fold  (s, Params::oscId (oscIndex1Based, "fold"),  "FOLD"),
          level (s, Params::oscId (oscIndex1Based, "level"), "LEVEL")
    {
        addAndMakeVisible (onSwitch);
        addAndMakeVisible (wave);
        addAndMakeVisible (pitch);
        addAndMakeVisible (fine);
        addAndMakeVisible (pwm);
        addAndMakeVisible (fold);
        addAndMakeVisible (level);
    }

    void resized() override
    {
        auto b = content();
        auto top = b.removeFromTop (22);
        onSwitch.setBounds (top.removeFromLeft (38));
        top.removeFromLeft (4);
        wave.setBounds (top);

        b.removeFromTop (3);
        const int kw = b.getWidth() / 5;
        pitch.setBounds (b.removeFromLeft (kw));
        fine.setBounds  (b.removeFromLeft (kw));
        pwm.setBounds   (b.removeFromLeft (kw));
        fold.setBounds  (b.removeFromLeft (kw));
        level.setBounds (b);
    }

private:
    SwitchToggle onSwitch;
    ChoiceCombo  wave;
    LabeledKnob  pitch, fine, pwm, fold, level;
};
