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
//  NoisePanel — the dedicated noise "oscillator": on/off, white<->pink
//  color blend, and level.
// ============================================================================

class NoisePanel : public SectionPanel
{
public:
    explicit NoisePanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Noise"),
          onSwitch (s, Params::id::noiseOn, ""),
          type (s, Params::id::noiseType),
          color (s, Params::id::noiseColor, "COLOR / CLOCK"),
          level (s, Params::id::noiseLevel, "LEVEL")
    {
        addAndMakeVisible (onSwitch);
        addAndMakeVisible (type);
        addAndMakeVisible (color);
        addAndMakeVisible (level);
    }

    void resized() override
    {
        auto b = content();
        auto top = b.removeFromTop (22);
        onSwitch.setBounds (top.removeFromLeft (38));
        top.removeFromLeft (4);
        type.setBounds (top);
        b.removeFromTop (2);

        const int kw = b.getWidth() / 2;
        color.setBounds (b.removeFromLeft (kw));
        level.setBounds (b);
    }

private:
    SwitchToggle onSwitch;
    ChoiceCombo type;
    LabeledKnob color, level;
};
