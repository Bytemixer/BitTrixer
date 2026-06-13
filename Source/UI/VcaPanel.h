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
//  VcaPanel — the output section: VCA drive ("preamp push"), compression
//  (density/punch), master volume, plus the lo-fi output format (sample
//  rate + bit depth) baked into the rendered/exported sound.
// ============================================================================

class VcaPanel : public SectionPanel
{
public:
    explicit VcaPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("VCA / Output"),
          drive  (s, Params::id::vcaDrive,  "DRIVE"),
          comp   (s, Params::id::comp,      "COMP"),
          master (s, Params::id::masterVol, "MASTER"),
          rate   (s, Params::id::outRate),
          bits   (s, Params::id::outBits, "8-BIT")
    {
        addAndMakeVisible (drive);
        addAndMakeVisible (comp);
        addAndMakeVisible (master);
        rateLabel.setText ("RATE", juce::dontSendNotification);
        rateLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
        rateLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
        addAndMakeVisible (rateLabel);
        addAndMakeVisible (rate);
        addAndMakeVisible (bits);
    }

    void resized() override
    {
        auto b = content();

        // output-format row at the bottom: RATE selector + 8-bit switch
        auto bottom = b.removeFromBottom (24);
        rateLabel.setBounds (bottom.removeFromLeft (32));
        rate.setBounds (bottom.removeFromLeft (94));
        bottom.removeFromLeft (6);
        bits.setBounds (bottom);

        // DRIVE / COMP / MASTER knobs
        const int kw = b.getWidth() / 3;
        drive.setBounds (b.removeFromLeft (kw));
        comp.setBounds  (b.removeFromLeft (kw));
        master.setBounds (b);
    }

private:
    LabeledKnob  drive, comp, master;
    juce::Label  rateLabel;
    ChoiceCombo  rate;
    SwitchToggle bits;
};
