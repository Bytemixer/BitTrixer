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
//  StepLfoPanel — LFO 2 is a step sequencer (the RetroForge twist). A
//  draggable bar-graph sets 2..8 bipolar step levels; routed through the mod
//  matrix (mapping it to pitch gives an arpeggio, to cutoff a rhythmic
//  filter, ...). Plus STEPS / RATE / GLIDE controls.
// ============================================================================

class StepLfoPanel : public SectionPanel
{
public:
    explicit StepLfoPanel (juce::AudioProcessorValueTreeState& s)
        : SectionPanel ("LFO 2 - Step Seq"),
          editor (s),
          steps (s, Params::id::stepCount, "STEPS"),
          rate  (s, Params::lfoId (2, "rate"), "RATE"),
          glide (s, Params::id::stepGlide, "GLIDE"),
          skew  (s, Params::id::stepSkew, "SKEW")
    {
        addAndMakeVisible (editor);
        addAndMakeVisible (steps);
        addAndMakeVisible (rate);
        addAndMakeVisible (glide);
        addAndMakeVisible (skew);
    }

    void resized() override
    {
        auto b = content();
        // the draggable step graph on top, four controls in a row beneath it
        auto knobs = b.removeFromBottom (60);
        const int kw = knobs.getWidth() / 4;
        steps.setBounds (knobs.removeFromLeft (kw));
        rate.setBounds  (knobs.removeFromLeft (kw));
        glide.setBounds (knobs.removeFromLeft (kw));
        skew.setBounds  (knobs);
        editor.setBounds (b.removeFromBottom (b.getHeight()).reduced (0, 2));
    }

private:
    // ---- the draggable bar-graph ----
    struct StepEditor : public juce::Component, private juce::Timer
    {
        explicit StepEditor (juce::AudioProcessorValueTreeState& s) : apvts (s)
        {
            countRaw = apvts.getRawParameterValue (Params::id::stepCount);
            for (int i = 0; i < Params::kMaxSteps; ++i)
                stepRaw[i] = apvts.getRawParameterValue (Params::stepValId (i + 1));
            startTimerHz (24);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (RetroColors::track);
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (RetroColors::panelEdge);
            g.drawRoundedRectangle (b, 4.0f, 1.0f);

            const auto r = b.reduced (3.0f);
            const float midY = r.getCentreY();
            g.setColour (RetroColors::textDim.withAlpha (0.35f));
            g.drawLine (r.getX(), midY, r.getRight(), midY, 1.0f);

            const int count = numSteps();
            const float colW = r.getWidth() / (float) count;
            for (int i = 0; i < count; ++i)
            {
                const float v = stepRaw[i] != nullptr ? stepRaw[i]->load() : 0.0f;
                const float x = r.getX() + (float) i * colW;
                auto cell = juce::Rectangle<float> (x + 1.5f, r.getY(), colW - 3.0f, r.getHeight());

                // the bar grows from the midline toward the level
                const float top = v >= 0.0f ? midY - v * (r.getHeight() * 0.5f) : midY;
                const float h   = std::fabs (v) * (r.getHeight() * 0.5f);
                g.setColour (RetroColors::accent.withAlpha (0.85f));
                g.fillRect (cell.getX(), top, cell.getWidth(), h < 1.0f ? 1.0f : h);
                g.setColour (RetroColors::accent);
                g.fillRect (cell.getX(), v >= 0.0f ? top : midY + h - 1.5f, cell.getWidth(), 1.5f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override { drag (e); }
        void mouseDrag (const juce::MouseEvent& e) override { drag (e); }

    private:
        int numSteps() const
        {
            return countRaw != nullptr ? juce::jlimit (2, Params::kMaxSteps, (int) countRaw->load()) : 4;
        }

        void drag (const juce::MouseEvent& e)
        {
            const auto r = getLocalBounds().toFloat().reduced (4.0f);
            const int count = numSteps();
            const int i = juce::jlimit (0, count - 1,
                                        (int) ((e.position.x - r.getX()) / (r.getWidth() / count)));
            float v = 1.0f - 2.0f * (e.position.y - r.getY()) / r.getHeight();
            v = juce::jlimit (-1.0f, 1.0f, v);
            if (auto* p = apvts.getParameter (Params::stepValId (i + 1)))
                p->setValueNotifyingHost (p->convertTo0to1 (v));
        }

        void timerCallback() override { repaint(); }

        juce::AudioProcessorValueTreeState& apvts;
        std::atomic<float>* countRaw = nullptr;
        std::atomic<float>* stepRaw[Params::kMaxSteps] {};
    };

    StepEditor   editor;
    LabeledKnob  steps, rate, glide, skew;
};
