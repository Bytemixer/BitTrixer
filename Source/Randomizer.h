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
#include <atomic>
#include <map>
#include <vector>
#include "Params.h"

// ============================================================================
//  Randomizer — sfxr/bfxr-style patch generation, message-thread only.
//  * fullRandom()      : anything-goes (within musical constraints)
//  * applyCategory()   : hand-tuned recipe ranges per SFX archetype
//  * variate()         : ANCHORED variation — the first press captures the
//                        current sound as an anchor; every later press
//                        re-perturbs FROM THE ANCHOR with tight, triangular
//                        nudges. Press it forever: you get siblings of the
//                        same sound, never a drift out of its family. The
//                        anchor refreshes automatically when anything else
//                        changes the sound (knob edit, category, undo...).
//  * mutate()          : compounding walk with wider swings — the explorer.
//  * undo()            : one-deep snapshot, taken before every action above
//  Writes through setValueNotifyingHost so the host/UI stay in sync.
//  Never touches: master volume, loop settings, MIDI track.
// ============================================================================

class Randomizer : private juce::AudioProcessorValueTreeState::Listener
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

    explicit Randomizer (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                watchedIds.add (rp->paramID);
                apvts.addParameterListener (rp->paramID, this);
            }
    }

    ~Randomizer() override
    {
        for (const auto& pid : watchedIds)
            apvts.removeParameterListener (pid, this);
    }

    void fullRandom();
    void applyCategory (Category c);
    void variate();
    void mutate();

    bool canUndo() const noexcept { return hasUndo; }
    void undo();

private:
    struct Nudge { juce::String pid; float scale; };

    void parameterChanged (const juce::String&, float) override
    {
        // a change we did not make ourselves = new sound family
        if (! selfChanging)
            anchorDirty.store (true);
    }

    void snapshotForUndo();
    void resetToNeutral();
    std::vector<Nudge> perturbTargets (bool includeLoudnessAndLength) const;
    void applyNudges (const std::vector<Nudge>& targets, float baseScale, bool fromAnchor);
    void refreshAnchorIfNeeded (const std::vector<Nudge>& targets);

    // real-value setters (converted through each parameter's range)
    void set (const juce::String& paramId, float realValue);
    void setChoice (const juce::String& paramId, int index);
    void setBool (const juce::String& paramId, bool on);

    // 4-operator FM voice presets (the engine in OSC 3's slot)
    void setFmVoice (int algo, float feedback, const float* ratios,
                     const float* levels, float outLevel);
    void fmChime (int modSlot);   // bell / coin / chime cascade
    void fmClang();               // metallic / inharmonic clang
    void fmNoise();               // op-1 feedback driven into noise

    void setFxOrder (const int* order);   // set the 7 chain-position params
    void shuffleFxOrder();                // random valid permutation

    void maybeFold (float prob);          // bias the OSC 1/2 wavefolder
    void addModSpice (int slot);          // a tasteful extra mod-matrix route

    float rnd (float lo, float hi)      { return juce::jmap (random.nextFloat(), lo, hi); }
    float rndLog (float lo, float hi)   { return lo * std::pow (hi / lo, random.nextFloat()); }
    bool  chance (float probability01)  { return random.nextFloat() < probability01; }
    int   rndInt (int lo, int hi)       { return random.nextInt ({ lo, hi + 1 }); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::Random random;
    juce::ValueTree undoState;
    bool hasUndo = false;

    juce::StringArray watchedIds;
    std::map<juce::String, float> anchor;     // normalized values per param
    std::atomic<bool> anchorDirty { true };
    bool selfChanging = false;
};
