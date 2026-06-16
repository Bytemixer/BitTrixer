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
//  ModMatrixPanel — six CV routing slots: source -> destination + depth.
// ============================================================================

class ModMatrixPanel : public SectionPanel
{
public:
    explicit ModMatrixPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Mod Matrix")
    {
        for (int k = 1; k <= Params::kNumModSlots; ++k)
        {
            auto row = std::make_unique<Row> (s, k);
            addAndMakeVisible (row->src);
            addAndMakeVisible (row->dest);
            addAndMakeVisible (row->depth);
            rows.push_back (std::move (row));
        }
    }

    void resized() override
    {
        auto b = content();
        const int rowH = b.getHeight() / Params::kNumModSlots;
        for (auto& row : rows)
        {
            auto r = b.removeFromTop (rowH).reduced (0, 4);                 // shorter rows -> space between them
            row->depth.setBounds (r.removeFromRight (r.getHeight() + 6));   // depth knob (grows with row height)
            r.removeFromRight (8);                                          // space to the left of the depth knob
            const int cw = r.getWidth() / 2;
            row->src.setBounds (r.removeFromLeft (cw).reduced (1, 0));
            row->dest.setBounds (r.reduced (1, 0));
        }
    }

private:
    struct Row
    {
        Row (juce::AudioProcessorValueTreeState& s, int k)
            : src (s, Params::modId (k, "src")),
              dest (s, Params::modId (k, "dest"))
        {
            // a bare rotary (no LabeledKnob wrapper -- its label area would
            // collapse the knob to nothing in these short rows).
            depth.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            depth.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            depth.setTooltip ("Depth");
            depthAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                s, Params::modId (k, "depth"), depth);
        }

        ChoiceCombo src, dest;
        juce::Slider depth;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt;
    };

    std::vector<std::unique_ptr<Row>> rows;
};
