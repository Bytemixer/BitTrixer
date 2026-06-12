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
//  PitchPanel — master pitch (base frequency, no keytracking by design) and
//  the unison-voice stack: voice count, detune spread, stereo spread.
// ============================================================================

class PitchPanel : public SectionPanel
{
public:
    explicit PitchPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("Pitch / Voices"),
          baseFreq (s, Params::id::baseFreq,  "BASE HZ"),
          voices   (s, Params::id::uniVoices, "VOICES"),
          detune   (s, Params::id::uniDetune, "DETUNE"),
          spread   (s, Params::id::uniSpread, "SPREAD"),
          midiTrack (s, Params::id::midiTrack, "MIDI PITCH"),
          sync     (s, Params::id::oscSync)
    {
        addAndMakeVisible (baseFreq);
        addAndMakeVisible (voices);
        addAndMakeVisible (detune);
        addAndMakeVisible (spread);
        addAndMakeVisible (midiTrack);

        syncLabel.setText ("SYNC", juce::dontSendNotification);
        syncLabel.setColour (juce::Label::textColourId, RetroColors::textDim);
        syncLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
        addAndMakeVisible (syncLabel);
        addAndMakeVisible (sync);
    }

    void resized() override
    {
        auto b = content();
        auto sw = b.removeFromBottom (20);
        midiTrack.setBounds (sw.removeFromLeft (130).withTrimmedLeft (4));
        syncLabel.setBounds (sw.removeFromLeft (38));
        sync.setBounds (sw.reduced (0, 0));

        const int kw = b.getWidth() / 4;
        baseFreq.setBounds (b.removeFromLeft (kw));
        voices.setBounds   (b.removeFromLeft (kw));
        detune.setBounds   (b.removeFromLeft (kw));
        spread.setBounds   (b);
    }

private:
    LabeledKnob baseFreq, voices, detune, spread;
    SwitchToggle midiTrack;
    juce::Label syncLabel;
    ChoiceCombo sync;
};
