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
          retrig (s, Params::id::retrigRate, "RETRIG"),
          autoVar (s, Params::id::autoVarOn, "AUTO"),
          varAmt (s, Params::id::autoVarAmt, "VAR AMT"),
          bits (s, Params::id::outBits, "8-BIT")
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
        addAndMakeVisible (retrig);
        addAndMakeVisible (autoVar);
        addAndMakeVisible (varAmt);
        addAndMakeVisible (bits);
    }

    void resized() override
    {
        auto b = content();
        // TRIGGER button with the 8-bit output switch tucked beneath it
        auto left = b.removeFromLeft (90);
        bits.setBounds (left.removeFromBottom (20));
        left.removeFromBottom (4);
        trigger.setBounds (left.reduced (2, 0));
        b.removeFromLeft (8);

        // a clean 4-knob row (reasonable size) over a switch/button row
        auto knobs = b.removeFromTop (b.getHeight() * 64 / 100);
        const int kw = knobs.getWidth() / 4;
        gate.setBounds     (knobs.removeFromLeft (kw));
        loopRate.setBounds (knobs.removeFromLeft (kw));
        retrig.setBounds   (knobs.removeFromLeft (kw));
        varAmt.setBounds   (knobs);

        auto sw = b.removeFromLeft (b.getWidth() / 2);
        loopOn.setBounds  (sw.removeFromTop (sw.getHeight() / 2).withSizeKeepingCentre (sw.getWidth() - 6, 18));
        autoVar.setBounds (sw.withSizeKeepingCentre (sw.getWidth() - 6, 18));
        auto btns = b.reduced (2, 1);
        variateButton.setBounds (btns.removeFromTop (btns.getHeight() / 2).reduced (0, 1));
        undoButton.setBounds (btns.reduced (0, 1));
    }

    std::function<void()> onTrigger, onVariate, onUndo;

private:
    juce::TextButton trigger;
    juce::TextButton variateButton, undoButton;
    LabeledKnob gate;
    SwitchToggle loopOn;
    LabeledKnob loopRate, retrig;
    SwitchToggle autoVar;
    LabeledKnob varAmt;
    SwitchToggle bits;
};
