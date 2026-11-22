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

#include <map>
#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  ShadowParams — an offline copy of every parameter's real value and range,
//  taken from the APVTS on the message thread. Edits to the copy never reach
//  the host, undo or the UI. Feeds a Params::Cache through raw().
// ============================================================================

class ShadowParams
{
public:
    explicit ShadowParams (juce::AudioProcessorValueTreeState& state)
    {
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (auto* raw = state.getRawParameterValue (rp->paramID))
                    entries.try_emplace (rp->paramID, rp->getNormalisableRange(), raw->load());
    }

    ShadowParams (const ShadowParams& other)
    {
        for (const auto& [pid, e] : other.entries)
            entries.try_emplace (pid, e.range, e.value.load());
    }

    ShadowParams& operator= (const ShadowParams&) = delete;

    // copies values only; both sides must hold the same parameter set
    void copyValuesFrom (const ShadowParams& other)
    {
        for (auto& [pid, e] : entries)
        {
            const auto it = other.entries.find (pid);
            if (it != other.entries.end())
                e.value.store (it->second.value.load());
        }
    }

    std::atomic<float>* raw (const juce::String& pid)
    {
        const auto it = entries.find (pid);
        return it != entries.end() ? &it->second.value : nullptr;
    }

    // same conversion as RangedAudioParameter::convertTo0to1 / convertFrom0to1
    bool getNormalised (const juce::String& pid, float& out) const
    {
        const auto it = entries.find (pid);
        if (it == entries.end())
            return false;
        const auto& r = it->second.range;
        out = r.convertTo0to1 (r.snapToLegalValue (it->second.value.load()));
        return true;
    }

    void setNormalised (const juce::String& pid, float normalised)
    {
        const auto it = entries.find (pid);
        if (it == entries.end())
            return;
        const auto& r = it->second.range;
        it->second.value.store (r.snapToLegalValue (
            r.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised))));
    }

private:
    struct Entry
    {
        Entry (const juce::NormalisableRange<float>& r, float v) : range (r), value (v) {}
        juce::NormalisableRange<float> range;
        std::atomic<float> value;
    };

    std::map<juce::String, Entry> entries;
};
