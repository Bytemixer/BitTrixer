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
//  VcaPanel — VCA drive ("preamp push") and master output volume.
// ============================================================================

class VcaPanel : public SectionPanel
{
public:
    explicit VcaPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("VCA / Output"),
          drive  (s, Params::id::vcaDrive,  "DRIVE"),
          master (s, Params::id::masterVol, "MASTER")
    {
        addAndMakeVisible (drive);
        addAndMakeVisible (master);
    }

    void resized() override
    {
        auto b = content();
        const int kw = b.getWidth() / 2;
        drive.setBounds (b.removeFromLeft (kw).reduced (12, 0));
        master.setBounds (b.reduced (12, 0));
    }

private:
    LabeledKnob drive, master;
};
