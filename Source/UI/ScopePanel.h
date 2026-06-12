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
#include "../DSP/SynthEngine.h"

// ============================================================================
//  ScopePanel — jsfxr-style oscilloscope: shows the whole waveform of the
//  last triggered sound (the engine restarts its capture on every trigger).
//  Draws min/max columns so long sounds stay readable.
// ============================================================================

class ScopePanel : public SectionPanel, private juce::Timer
{
public:
    explicit ScopePanel (const SynthEngine& engineToWatch)
        : SectionPanel ("Scope"), engine (engineToWatch)
    {
        startTimerHz (30);
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

        const int avail = engine.scopeAvailable();
        if (avail < 2)
            return;

        const float* data = engine.scopeData();
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
                const float s = data[i];
                lo = juce::jmin (lo, s);
                hi = juce::jmax (hi, s);
            }
            const float x = r.getX() + (float) c;
            wave.addLineSegment ({ x, midY - hi * halfH, x, midY - lo * halfH }, 1.0f);
        }

        g.setColour (RetroColors::accent.withAlpha (0.9f));
        g.fillPath (wave);
    }

private:
    void timerCallback() override
    {
        const auto gen = engine.scopeGeneration();
        const auto avail = engine.scopeAvailable();
        if (gen != lastGen || avail != lastAvail)
        {
            lastGen = gen;
            lastAvail = avail;
            repaint();
        }
    }

    const SynthEngine& engine;
    uint32_t lastGen = 0;
    int lastAvail = 0;
};
