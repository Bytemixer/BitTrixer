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

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"

// ============================================================================
//  Randomizer — sfxr/bfxr-style patch generation, message-thread only.
//  * fullRandom()      : anything-goes (within musical constraints)
//  * applyCategory()   : hand-tuned recipe ranges per SFX archetype
//  * variate()         : perturbs the CURRENT patch by the Variate Amount
//                        parameter — an editable "sibling" of the sound
//  * undo()            : one-deep snapshot, taken before every action above
//  Writes through setValueNotifyingHost so the host/UI stay in sync.
//  Never touches: master volume, loop settings, gate, MIDI track.
// ============================================================================

class Randomizer
{
public:
    enum class Category { Pickup = 0, Laser, Explosion, Powerup, Hit, Jump, Blip, OneUp, Lose };
    static constexpr int kNumCategories = 9;

    static const char* categoryName (Category c)
    {
        switch (c)
        {
            case Category::Pickup:    return "PICKUP";
            case Category::Laser:     return "LASER";
            case Category::Explosion: return "EXPLODE";
            case Category::Powerup:   return "POWERUP";
            case Category::Hit:       return "HIT";
            case Category::Jump:      return "JUMP";
            case Category::Blip:      return "BLIP";
            case Category::OneUp:     return "1-UP";
            case Category::Lose:      return "LOSE";
        }
        return "?";
    }

    explicit Randomizer (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

    void fullRandom();
    void applyCategory (Category c);
    void variate();

    bool canUndo() const noexcept { return hasUndo; }
    void undo();

private:
    void snapshotForUndo();
    void resetToNeutral();

    // real-value setters (converted through each parameter's range)
    void set (const juce::String& paramId, float realValue);
    void setChoice (const juce::String& paramId, int index);
    void setBool (const juce::String& paramId, bool on);

    float rnd (float lo, float hi)      { return juce::jmap (random.nextFloat(), lo, hi); }
    float rndLog (float lo, float hi)   { return lo * std::pow (hi / lo, random.nextFloat()); }
    bool  chance (float probability01)  { return random.nextFloat() < probability01; }
    int   rndInt (int lo, int hi)       { return random.nextInt ({ lo, hi + 1 }); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Random random;
    juce::ValueTree undoState;
    bool hasUndo = false;
};
