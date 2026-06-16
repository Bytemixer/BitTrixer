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
          j1Amt  (s, Params::id::pj1Amt,  "JUMP1 ST"),
          j1Time (s, Params::id::pj1Time, "JUMP1 AT"),
          j2Amt  (s, Params::id::pj2Amt,  "JUMP2 ST"),
          j2Time (s, Params::id::pj2Time, "JUMP2 AT")
    {
        addAndMakeVisible (baseFreq);
        addAndMakeVisible (voices);
        addAndMakeVisible (detune);
        addAndMakeVisible (spread);
        addAndMakeVisible (j1Amt);
        addAndMakeVisible (j1Time);
        addAndMakeVisible (j2Amt);
        addAndMakeVisible (j2Time);
        addAndMakeVisible (midiTrack);
    }

    void resized() override
    {
        auto b = content();
        auto sw = b.removeFromTop (20);
        midiTrack.setBounds (sw.withTrimmedLeft (4));

        auto row1 = b.removeFromTop (b.getHeight() / 2);
        const int kw = row1.getWidth() / 4;
        baseFreq.setBounds (row1.removeFromLeft (kw));
        voices.setBounds   (row1.removeFromLeft (kw));
        detune.setBounds   (row1.removeFromLeft (kw));
        spread.setBounds   (row1);

        const int kw2 = b.getWidth() / 4;
        j1Amt.setBounds  (b.removeFromLeft (kw2));
        j1Time.setBounds (b.removeFromLeft (kw2));
        j2Amt.setBounds  (b.removeFromLeft (kw2));
        j2Time.setBounds (b);
    }

private:
    LabeledKnob baseFreq, voices, detune, spread;
    SwitchToggle midiTrack;
    LabeledKnob j1Amt, j1Time, j2Amt, j2Time;
};
