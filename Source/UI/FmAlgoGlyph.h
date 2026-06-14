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
#include "../DSP/FmVoice.h"

// ============================================================================
//  FmAlgoGlyph — a small recessed window that diagrams the selected FM
//  algorithm: four operator boxes wired by their modulation routing, with the
//  carriers (the boxes that reach the output) highlighted. Box positions are
//  derived from each operator's distance to the output, so all eight YM2612
//  algorithms draw automatically from FmUnit::kAlgos. Repaints on change.
// ============================================================================

class FmAlgoGlyph : public juce::Component, private juce::Timer
{
public:
    explicit FmAlgoGlyph (juce::AudioProcessorValueTreeState& s)
    {
        algoParam = s.getRawParameterValue (Params::id::fmAlgo);
        startTimerHz (8);
    }
    ~FmAlgoGlyph() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const int a = algoParam != nullptr ? (int) (algoParam->load() + 0.5f) : 0;
        const auto& al = FmUnit::kAlgos[(size_t) juce::jlimit (0, FmUnit::kNumAlgos - 1, a)];

        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (RetroColors::track);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (RetroColors::panelEdge);
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        // each operator's "level" = how many hops it is above the output
        // (carrier = 0). Modulators are lower-indexed than what they feed, so a
        // single high->low sweep resolves every level.
        int level[4] {}; int maxLevel = 0;
        for (int i = 3; i >= 0; --i)
        {
            if (al.carriers & (1u << i)) { level[i] = 0; continue; }
            int mx = 1;
            for (int j = 0; j < 4; ++j)
                if (al.mod[(size_t) j] & (1u << i)) mx = juce::jmax (mx, level[j] + 1);
            level[i] = mx;
            maxLevel = juce::jmax (maxLevel, mx);
        }

        // lay the boxes out: row by level (carriers on the bottom row), spread
        // evenly across each row
        auto inner = r.reduced (4.0f);
        const float boxW = juce::jmin (14.0f, inner.getWidth() / 4.2f);
        const float boxH = juce::jmin (13.0f, inner.getHeight() / (float) (maxLevel + 1) - 2.0f);
        juce::Point<float> centre[4];

        for (int lv = 0; lv <= maxLevel; ++lv)
        {
            int idsAtLv[4] {}; int n = 0;
            for (int i = 0; i < 4; ++i) if (level[i] == lv) idsAtLv[n++] = i;
            const float y = inner.getBottom() - boxH * 0.5f
                          - (float) lv * (inner.getHeight() - boxH) / (float) juce::jmax (1, maxLevel);
            for (int k = 0; k < n; ++k)
            {
                const float x = inner.getX() + inner.getWidth() * (float) (k + 1) / (float) (n + 1);
                centre[idsAtLv[k]] = { x, y };
            }
        }

        // modulation lines first, then the boxes on top
        g.setColour (RetroColors::textDim.withAlpha (0.85f));
        for (int dst = 0; dst < 4; ++dst)
            for (int srcOp = 0; srcOp < 4; ++srcOp)
                if (al.mod[(size_t) dst] & (1u << srcOp))
                    g.drawLine (centre[srcOp].x, centre[srcOp].y, centre[dst].x, centre[dst].y, 1.0f);

        g.setFont (juce::Font (juce::FontOptions (boxH - 2.0f, juce::Font::plain)));
        for (int i = 0; i < 4; ++i)
        {
            const bool carrier = (al.carriers & (1u << i)) != 0;
            juce::Rectangle<float> box (centre[i].x - boxW * 0.5f, centre[i].y - boxH * 0.5f, boxW, boxH);
            g.setColour (carrier ? RetroColors::accent : RetroColors::panel.brighter (0.25f));
            g.fillRoundedRectangle (box, 1.5f);
            g.setColour (carrier ? RetroColors::accentDark : RetroColors::panelEdge);
            g.drawRoundedRectangle (box, 1.5f, 1.0f);
            g.setColour (carrier ? juce::Colour (0xff10151c) : RetroColors::text);
            g.drawText (juce::String (i + 1), box, juce::Justification::centred);
        }
    }

private:
    void timerCallback() override
    {
        const float v = algoParam != nullptr ? algoParam->load() : 0.0f;
        if (v != lastAlgo) { lastAlgo = v; repaint(); }
    }

    std::atomic<float>* algoParam = nullptr;
    float lastAlgo = -1.0f;
};
