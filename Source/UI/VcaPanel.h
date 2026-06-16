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
//  VcaPanel — the output section: VCA drive ("preamp push"), compression
//  (density/punch) and master volume. (Output sample rate lives in the top
//  bar; the 8-bit switch lives by the Trigger button.)
// ============================================================================

class VcaPanel : public SectionPanel
{
public:
    explicit VcaPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("VCA / Output"),
          drive  (s, Params::id::vcaDrive,  "DRIVE"),
          comp   (s, Params::id::comp,      "COMP"),
          master (s, Params::id::masterVol, "MASTER")
    {
        addAndMakeVisible (drive);
        addAndMakeVisible (comp);
        addAndMakeVisible (master);
    }

    void resized() override
    {
        auto b = content();
        const int kw = b.getWidth() / 3;
        drive.setBounds (b.removeFromLeft (kw));
        comp.setBounds  (b.removeFromLeft (kw));
        master.setBounds (b);
    }

private:
    LabeledKnob drive, comp, master;
};
