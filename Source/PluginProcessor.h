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
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

#include "Params.h"
#include "DSP/SynthEngine.h"

// ============================================================================
//  RetroForgeProcessor
//  The JUCE AudioProcessor shell. Owns the parameter tree (APVTS) and the
//  standalone synth engine. processBlock snapshots the parameters once per
//  block, consumes UI trigger requests, and hands MIDI gating + rendering
//  to the engine.
// ============================================================================

class RetroForgeProcessor : public juce::AudioProcessor
{
public:
    RetroForgeProcessor();
    ~RetroForgeProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "RetroForge"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- UI -> audio thread trigger requests (lock-free) ----
    void uiGate (bool on) noexcept       { uiGateRequest.store (on ? 1 : 2); }
    void uiOneShot() noexcept            { uiOneShotRequests.fetch_add (1); }

    // Snapshot of the current panel for offline rendering (WAV export, scope).
    Params::Patch snapshotPatch() const  { return paramCache.read(); }

private:
    Params::Cache paramCache;
    SynthEngine engine;

    std::atomic<int> uiGateRequest { 0 };      // 0 none, 1 on, 2 off
    std::atomic<int> uiOneShotRequests { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroForgeProcessor)
};
