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
#include "../WavExporter.h"

// ============================================================================
//  ScopePanel — jsfxr-style STATIC waveform: the full sound is offline-
//  rendered and drawn at once, re-rendering (debounced) whenever any
//  parameter changes — knob tweaks, trigger, category/random/variate.
// ============================================================================

class ScopePanel : public SectionPanel,
                   private juce::Timer,
                   private juce::AudioProcessorValueTreeState::Listener
{
public:
    ScopePanel (juce::AudioProcessorValueTreeState& state,
                std::function<Params::Patch()> patchProvider)
        : SectionPanel ("Scope"), apvts (state), getPatch (std::move (patchProvider)),
          renderer (44100.0)
    {
        for (auto* p : apvts.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                watchedIds.add (rp->paramID);
                apvts.addParameterListener (rp->paramID, this);
            }

        lastChangeMs.store (juce::Time::getMillisecondCounter() - 1000);
        dirty.store (true);
        startTimerHz (15);
    }

    ~ScopePanel() override
    {
        for (const auto& pid : watchedIds)
            apvts.removeParameterListener (pid, this);
    }

    void paint (juce::Graphics& g) override
    {
        SectionPanel::paint (g);

        auto b = content().toFloat().reduced (2.0f);
        g.setColour (RetroColors::track);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (RetroColors::panelEdge);
        g.drawRoundedRectangle (b, 4.0f, 1.0f);

        const auto r = b.reduced (3.0f, 3.0f);
        const float midY = r.getCentreY();

        g.setColour (RetroColors::textDim.withAlpha (0.35f));
        g.drawLine (r.getX(), midY, r.getRight(), midY, 1.0f);

        const int avail = (int) mono.size();
        if (avail < 2)
            return;

        const int cols = juce::jmax (1, (int) r.getWidth());
        const float samplesPerCol = (float) avail / (float) cols;
        const float halfH = r.getHeight() * 0.5f;

        juce::Path wave;
        for (int c = 0; c < cols; ++c)
        {
            const int i0 = (int) ((float) c * samplesPerCol);
            const int i1 = juce::jmin (avail, juce::jmax (i0 + 1, (int) ((float) (c + 1) * samplesPerCol)));
            float lo = 1.0f, hi = -1.0f;
            for (int i = i0; i < i1; ++i)
            {
                const float s = mono[(size_t) i];
                lo = juce::jmin (lo, s);
                hi = juce::jmax (hi, s);
            }
            const float x = r.getX() + (float) c;
            wave.addLineSegment ({ x, midY - hi * halfH, x, midY - lo * halfH }, 1.0f);
        }

        g.setColour (RetroColors::accent.withAlpha (0.9f));
        g.fillPath (wave);

        // duration readout
        g.setColour (RetroColors::textDim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (juce::String (avail / 44100.0, 2) + " s",
                    content().reduced (6, 4), juce::Justification::topRight);
    }

private:
    void parameterChanged (const juce::String&, float) override
    {
        // may arrive from the audio thread — atomics only
        lastChangeMs.store (juce::Time::getMillisecondCounter());
        dirty.store (true);
    }

    void timerCallback() override
    {
        if (! dirty.load())
            return;
        if (juce::Time::getMillisecondCounter() - lastChangeMs.load() < 250)
            return;

        dirty.store (false);
        renderWaveform();
        repaint();
    }

    void renderWaveform()
    {
        const auto buffer = renderer.renderPatch (getPatch(), 5.0);
        const int n = buffer.getNumSamples();
        mono.resize ((size_t) n);
        const float* l = buffer.getReadPointer (0);
        const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = 0.5f * (l[i] + r[i]);
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::function<Params::Patch()> getPatch;
    WavExporter renderer;
    juce::StringArray watchedIds;
    std::vector<float> mono;
    std::atomic<bool> dirty { false };
    std::atomic<juce::uint32> lastChangeMs { 0 };
};
