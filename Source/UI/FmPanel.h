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
//  FmPanel — the 4-operator FM voice that occupies the 3rd generator slot.
//  Header: enable + algorithm selector. Globals: PITCH / FINE (transpose) +
//  LEVEL (output) + FEEDBACK (operator 1). Then a 4-column operator grid of
//  RATIO over LEVEL. Enable/pitch/fine/level reuse the OSC 3 params, so the
//  matrix's "Osc3 Pitch/Level" destinations stay meaningful; the operators are
//  animated through the new FM mod-matrix destinations.
// ============================================================================

class FmPanel : public SectionPanel
{
public:
    explicit FmPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("FM  (4-Op)"),
          onSwitch (s, Params::oscId (3, "on"), ""),
          algo     (s, Params::id::fmAlgo),
          pitch    (s, Params::oscId (3, "pitch"), "PITCH"),
          fine     (s, Params::oscId (3, "fine"),  "FINE"),
          outLevel (s, Params::oscId (3, "level"), "LEVEL"),
          feedback (s, Params::id::fmFeedback,      "FDBK")
    {
        addAndMakeVisible (onSwitch);
        addAndMakeVisible (algo);
        for (auto* k : { &pitch, &fine, &outLevel, &feedback })
            addAndMakeVisible (k);

        static const char* opNames[4] = { "OP1", "OP2", "OP3", "OP4" };
        for (int i = 0; i < 4; ++i)
        {
            ops[i].ratio = std::make_unique<LabeledKnob> (s, Params::fmOpId (i + 1, "ratio"), opNames[i]);
            ops[i].level = std::make_unique<LabeledKnob> (s, Params::fmOpId (i + 1, "level"), "LVL");
            addAndMakeVisible (*ops[i].ratio);
            addAndMakeVisible (*ops[i].level);
        }
    }

    void resized() override
    {
        auto b = content();

        // header: enable + algorithm selector
        auto top = b.removeFromTop (22);
        onSwitch.setBounds (top.removeFromLeft (32));
        top.removeFromLeft (4);
        algo.setBounds (top);
        b.removeFromTop (4);

        // global row: PITCH / FINE / LEVEL / FEEDBACK
        auto glob = b.removeFromTop (66);
        const int gw = glob.getWidth() / 4;
        pitch.setBounds    (glob.removeFromLeft (gw));
        fine.setBounds     (glob.removeFromLeft (gw));
        outLevel.setBounds (glob.removeFromLeft (gw));
        feedback.setBounds (glob);
        b.removeFromTop (2);

        // operator grid: 4 columns, RATIO over LEVEL
        const int cw = b.getWidth() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto c = b.removeFromLeft (i == 3 ? b.getWidth() : cw);
            const int half = c.getHeight() / 2;
            ops[i].ratio->setBounds (c.removeFromTop (half));
            ops[i].level->setBounds (c);
        }
    }

private:
    struct OpStrip { std::unique_ptr<LabeledKnob> ratio, level; };

    SwitchToggle onSwitch;
    ChoiceCombo  algo;
    LabeledKnob  pitch, fine, outLevel, feedback;
    OpStrip      ops[4];
};
