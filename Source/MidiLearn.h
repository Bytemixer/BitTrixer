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

// ============================================================================
//  MidiLearn — MIDI-CC mapping + learn for the parameter tree.
//
//  * A CC (0-127) maps to one parameter; turning that hardware control sets the
//    parameter. Incoming CC values are captured on the audio thread but applied
//    on the message thread (via AsyncUpdater) so the host/UI stay in sync and
//    no host callback runs on the RT thread.
//  * Learn: arm, then the next incoming CC binds to the LAST parameter the user
//    touched in the UI (tracked via gesture begins). One bind disarms.
//  * Mappings persist in the plugin state.
// ============================================================================

class MidiLearn : public juce::AsyncUpdater,
                  private juce::AudioProcessorParameter::Listener
{
public:
    explicit MidiLearn (juce::AudioProcessorValueTreeState& s) : apvts (s)
    {
        for (auto* p : apvts.processor.getParameters())
            p->addListener (this);
    }

    ~MidiLearn() override
    {
        cancelPendingUpdate();
        for (auto* p : apvts.processor.getParameters())
            p->removeListener (this);
    }

    // ---- audio thread: feed every incoming controller message here ----
    void handleCc (int cc, int value) noexcept
    {
        if (cc < 0 || cc >= 128) return;

        if (armed.load (std::memory_order_relaxed))
        {
            if (auto* tgt = lastTouched.load())     // bind to the last-touched param
            {
                byCc[(size_t) cc].store (tgt);
                armed.store (false);
                learnedCc.store (cc);
                learnGen.fetch_add (1);
                learnFired.store (true);
                triggerAsyncUpdate();
            }
            return;                                 // don't jump the value on the learn move
        }

        if (byCc[(size_t) cc].load() != nullptr)
        {
            pendingVal[(size_t) cc].store (value * (1.0f / 127.0f));
            pendingDirty[(size_t) cc].store (true);
            anyDirty.store (true);
            triggerAsyncUpdate();
        }
    }

    // ---- message-thread API ----
    void setArmed (bool a)     { armed.store (a); notify(); }
    bool isArmed() const       { return armed.load(); }
    void clearAll()            { for (auto& p : byCc) p.store (nullptr); notify(); }
    int  mappingCount() const  { int c = 0; for (auto& p : byCc) if (p.load()) ++c; return c; }
    int  lastLearnedCc() const { return learnedCc.load(); }
    int  learnGeneration() const { return learnGen.load(); }
    juce::RangedAudioParameter* paramForCc (int cc) const
        { return (cc >= 0 && cc < 128) ? byCc[(size_t) cc].load() : nullptr; }

    std::function<void()> onChanged;        // armed / learn / clear -> UI refresh

    // ---- persistence (appends/reads a MIDIMAP child on the given tree) ----
    void saveTo (juce::ValueTree& parent) const
    {
        juce::ValueTree mm ("MIDIMAP");
        for (int cc = 0; cc < 128; ++cc)
            if (auto* p = byCc[(size_t) cc].load())
            {
                juce::ValueTree e ("CC");
                e.setProperty ("n", cc, nullptr);
                e.setProperty ("p", p->paramID, nullptr);
                mm.appendChild (e, nullptr);
            }
        parent.appendChild (mm, nullptr);
    }

    void loadFrom (const juce::ValueTree& parent)
    {
        for (auto& p : byCc) p.store (nullptr);
        auto mm = parent.getChildWithName ("MIDIMAP");
        for (int i = 0; i < mm.getNumChildren(); ++i)
        {
            auto e = mm.getChild (i);
            const int cc = (int) e.getProperty ("n", -1);
            const auto pid = e.getProperty ("p").toString();
            if (cc >= 0 && cc < 128)
                if (auto* prm = apvts.getParameter (pid))
                    byCc[(size_t) cc].store (prm);
        }
        notify();
    }

    void handleAsyncUpdate() override
    {
        if (anyDirty.exchange (false))
            for (int cc = 0; cc < 128; ++cc)
                if (pendingDirty[(size_t) cc].exchange (false))
                    if (auto* p = byCc[(size_t) cc].load())
                        p->setValueNotifyingHost (pendingVal[(size_t) cc].load());

        if (learnFired.exchange (false))
            notify();
    }

private:
    void notify() { if (onChanged) onChanged(); }

    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int idx, bool starting) override
    {
        if (! starting) return;
        const auto& params = apvts.processor.getParameters();
        if (idx >= 0 && idx < params.size())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (params[idx]))
                lastTouched.store (rp);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<juce::RangedAudioParameter*> byCc[128] {};
    std::atomic<float> pendingVal[128] {};
    std::atomic<bool>  pendingDirty[128] {};
    std::atomic<bool>  anyDirty { false };
    std::atomic<bool>  armed { false };
    std::atomic<bool>  learnFired { false };
    std::atomic<juce::RangedAudioParameter*> lastTouched { nullptr };
    std::atomic<int>   learnedCc { -1 };
    std::atomic<int>   learnGen { 0 };
};
