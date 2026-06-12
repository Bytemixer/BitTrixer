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
//  TriggerPanel — the play surface: TRIGGER fires the complete sound as a
//  one-shot (gate length = the GATE knob; the full envelope cycle always
//  plays, regardless of how briefly the mouse is held). Plus gate time,
//  loop mode and the variate cluster. The owner wires onTrigger / onVariate
//  / onUndo.
// ============================================================================

class TriggerPanel : public SectionPanel
{
public:
    explicit TriggerPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Trigger"),
          gate (s, Params::id::gateTime, "GATE"),
          loopOn (s, Params::id::loopOn, "LOOP"),
          loopRate (s, Params::id::loopRate, "INTERVAL"),
          autoVar (s, Params::id::autoVarOn, "AUTO"),
          varAmt (s, Params::id::autoVarAmt, "VAR AMT")
    {
        trigger.setButtonText ("TRIGGER");
        trigger.setColour (juce::TextButton::buttonColourId, RetroColors::accentDark);
        trigger.onClick = [this] { if (onTrigger) onTrigger(); };
        addAndMakeVisible (trigger);

        variateButton.setButtonText ("VARIATE");
        variateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8a63d4));
        variateButton.onClick = [this] { if (onVariate) onVariate(); };
        addAndMakeVisible (variateButton);

        undoButton.setButtonText ("UNDO");
        undoButton.onClick = [this] { if (onUndo) onUndo(); };
        addAndMakeVisible (undoButton);

        addAndMakeVisible (gate);
        addAndMakeVisible (loopOn);
        addAndMakeVisible (loopRate);
        addAndMakeVisible (autoVar);
        addAndMakeVisible (varAmt);
    }

    void resized() override
    {
        auto b = content();
        trigger.setBounds (b.removeFromLeft (108).reduced (2));
        b.removeFromLeft (6);

        auto top = b.removeFromTop (b.getHeight() / 2);
        const int cw = top.getWidth() / 3;
        gate.setBounds (top.removeFromLeft (cw));
        loopRate.setBounds (top.removeFromLeft (cw));
        loopOn.setBounds (top.withSizeKeepingCentre (top.getWidth(), 20));

        const int cw2 = b.getWidth() / 3;
        varAmt.setBounds (b.removeFromLeft (cw2));
        auto sw = b.removeFromLeft (cw2);
        autoVar.setBounds (sw.withSizeKeepingCentre (sw.getWidth(), 20));
        auto btns = b.reduced (2, 4);
        variateButton.setBounds (btns.removeFromTop (btns.getHeight() / 2).reduced (0, 2));
        undoButton.setBounds (btns.reduced (0, 2));
    }

    std::function<void()> onTrigger, onVariate, onUndo;

private:
    juce::TextButton trigger;
    juce::TextButton variateButton, undoButton;
    LabeledKnob gate;
    SwitchToggle loopOn;
    LabeledKnob loopRate;
    SwitchToggle autoVar;
    LabeledKnob varAmt;
};
