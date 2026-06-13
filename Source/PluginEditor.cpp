/*  This file is part of the RetroForge audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "PluginEditor.h"

RetroForgeEditor::RetroForgeEditor (RetroForgeProcessor& p)
    : AudioProcessorEditor (p), proc (p),
      randomizer (p.apvts),
      presetManager (p.apvts),
      osc1 (p.apvts, 1), osc2 (p.apvts, 2), osc3 (p.apvts, 3),
      noisePanel (p.apvts),
      filterPanel (p.apvts),
      envFPanel (p.apvts, "Filter Envelope",
                 Params::id::envFAttack, Params::id::envFDecay,
                 Params::id::envFSustain, Params::id::envFRelease,
                 Params::id::envFCurve, Params::id::envFInvert),
      envAPanel (p.apvts, "Amp Envelope",
                 Params::id::envAAttack, Params::id::envADecay,
                 Params::id::envASustain, Params::id::envARelease,
                 Params::id::envACurve, Params::id::envAInvert),
      vcaPanel (p.apvts),
      pitchPanel (p.apvts),
      lfo1Panel (p.apvts, 1), lfo2Panel (p.apvts, 2),
      modMatrixPanel (p.apvts),
      triggerPanel (p.apvts),
      scopePanel (p.apvts, [&p] { return p.snapshotPatch(); }),
      fxPanel (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &header, &osc1, &osc2, &osc3, &noisePanel, &filterPanel,
             &envFPanel, &envAPanel, &vcaPanel, &pitchPanel,
             &lfo1Panel, &lfo2Panel, &modMatrixPanel,
             &triggerPanel, &randomizerPanel, &scopePanel, &fxPanel })
        addAndMakeVisible (c);

    // ---- wiring ----
    triggerPanel.onTrigger = [this] { proc.uiOneShot(); };
    triggerPanel.onVariate = [this] { randomizer.variate(); previewSound(); };
    triggerPanel.onUndo    = [this] { randomizer.undo(); previewSound(); };

    randomizerPanel.onMutate = [this] { randomizer.mutate(); previewSound(); };

    randomizerPanel.onRandom = [this]
    {
        randomizer.fullRandom();
        presetManager.setCurrentName ("Random");
        header.setPresetName ("Random");
        previewSound();
    };
    randomizerPanel.onCategory = [this] (int index)
    {
        const auto cat = (Randomizer::Category) index;
        randomizer.applyCategory (cat);
        const juce::String name = Randomizer::categoryName (cat);
        presetManager.setCurrentName (name);
        header.setPresetName (name);
        previewSound();
    };

    header.onSave = [this] { presetManager.saveAsync(); };
    header.onLoad = [this] { presetManager.loadAsync(); };
    presetManager.onPresetChanged = [this]
    {
        header.setPresetName (presetManager.getCurrentName());
    };

    header.onExport = [this]
    {
        const double sr = proc.getSampleRate();
        wavExporter.setSampleRate (sr > 8000.0 ? sr : 44100.0);
        wavExporter.exportAsync (proc.snapshotPatch());
    };

    header.onAbout = [this]
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "RetroForge",
            "RetroForge - Retro Game SFX Synthesizer\n"
            "Copyright (C) 2026 Bytemixer\n\n"
            "Licensed under the GNU Affero General Public License v3.0 or later.\n"
            "This program comes with ABSOLUTELY NO WARRANTY.\n"
            "See the LICENSE file for details.",
            "OK", this);
    };

    // ---- theme system ----
    header.setThemeNames (themeManager.names(), themeManager.currentIndex());
    header.onThemeSelected = [this] (int index)
    {
        themeManager.applyIndex (index);
        if (index == ThemeManager::kCustomIndex)
        {
            themeEditor.setVisible (true);
            themeEditor.toFront (true);
        }
    };
    themeManager.onThemeChanged = [this]
    {
        lookAndFeel.applyThemeColours();
        sendLookAndFeelChange();
        repaint();
    };
    themeEditor.setVisible (false);
    addChildComponent (themeEditor);
    themeManager.onThemeChanged();   // apply persisted theme on open

    header.setPresetName (presetManager.getCurrentName());
    setSize (1180, 850);
}

RetroForgeEditor::~RetroForgeEditor()
{
    setLookAndFeel (nullptr);
}

void RetroForgeEditor::previewSound()
{
    proc.uiOneShot();
}

void RetroForgeEditor::paint (juce::Graphics& g)
{
    g.fillAll (RetroColors::background);
    drawSignalTraces (g);
}

// ============================================================================
//  PCB-style signal routing drawn on the background between the panels:
//  solder pads where a signal leaves a section, arrowheads where it enters.
//  Solid traces = audio path (sources -> VCF -> VCA -> FX -> OUT),
//  dashed thin traces = modulation (LFOs/envelopes -> matrix -> VCF).
// ============================================================================

namespace
{
    void strokeTrace (juce::Graphics& g, const juce::Path& p, juce::Colour c,
                      float width, bool dashed = false)
    {
        g.setColour (c);
        if (dashed)
        {
            juce::Path d;
            const float dashes[2] = { 5.0f, 4.0f };
            juce::PathStrokeType (width).createDashedStroke (d, p, dashes, 2);
            g.fillPath (d);
        }
        else
        {
            g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        }
    }

    void solderPad (juce::Graphics& g, juce::Point<float> pt, juce::Colour c)
    {
        g.setColour (c);
        g.fillEllipse (pt.x - 3.5f, pt.y - 3.5f, 7.0f, 7.0f);
        g.setColour (RetroColors::background);
        g.fillEllipse (pt.x - 1.3f, pt.y - 1.3f, 2.6f, 2.6f);
    }

    // dir: 0 = pointing right, 1 = left, 2 = down, 3 = up
    void arrowInto (juce::Graphics& g, juce::Point<float> tip, int dir, juce::Colour c)
    {
        juce::Path a;
        constexpr float l = 7.0f, hw = 4.5f;
        switch (dir)
        {
            case 0: a.addTriangle (tip.x - l, tip.y - hw, tip.x - l, tip.y + hw, tip.x, tip.y); break;
            case 1: a.addTriangle (tip.x + l, tip.y - hw, tip.x + l, tip.y + hw, tip.x, tip.y); break;
            case 2: a.addTriangle (tip.x - hw, tip.y - l, tip.x + hw, tip.y - l, tip.x, tip.y); break;
            default: a.addTriangle (tip.x - hw, tip.y + l, tip.x + hw, tip.y + l, tip.x, tip.y); break;
        }
        g.setColour (c);
        g.fillPath (a);
    }

    // PCB-style polyline: corners are 45-degree chamfers, never square turns
    juce::Path chamfered (std::initializer_list<juce::Point<float>> waypoints,
                          float ch = 8.0f)
    {
        std::vector<juce::Point<float>> pts (waypoints);
        juce::Path p;
        if (pts.size() < 2)
            return p;
        p.startNewSubPath (pts.front());
        for (size_t i = 1; i + 1 < pts.size(); ++i)
        {
            const auto d1 = pts[i] - pts[i - 1];
            const auto d2 = pts[i + 1] - pts[i];
            const float l1 = std::sqrt (d1.x * d1.x + d1.y * d1.y);
            const float l2 = std::sqrt (d2.x * d2.x + d2.y * d2.y);
            const float c = juce::jmin (ch, l1 * 0.5f, l2 * 0.5f);
            if (l1 > 0.01f) p.lineTo (pts[i] - d1 * (c / l1));
            if (l2 > 0.01f) p.lineTo (pts[i] + d2 * (c / l2));
        }
        p.lineTo (pts.back());
        return p;
    }

    // junction via: where a branch meets a bus
    void via (juce::Graphics& g, juce::Point<float> pt, juce::Colour c)
    {
        g.setColour (c);
        g.fillEllipse (pt.x - 2.6f, pt.y - 2.6f, 5.2f, 5.2f);
        g.setColour (RetroColors::background);
        g.fillEllipse (pt.x - 1.0f, pt.y - 1.0f, 2.0f, 2.0f);
    }
}

void RetroForgeEditor::drawSignalTraces (juce::Graphics& g)
{
    // pride theme: the signal path becomes a rainbow ribbon — each leg of
    // the audio chain takes the next flag stripe, input to output
    const bool pride = RetroColors::prideMode;
    auto leg = [pride] (int i) {
        return pride ? RetroColors::kPrideFlag[i].brighter (0.15f) : RetroColors::trace;
    };
    const auto srcCol    = leg (0);                     // red    : sources -> CRUSH
    const auto vcfCol    = leg (1);                     // orange : CRUSH -> VCF
    const auto vcaCol    = leg (2);                     // yellow : VCF -> VCA
    const auto phaseCol  = leg (3);                     // green  : VCA -> PHASER
    const auto outCol    = leg (4);                     // blue   : FLANGER -> OUT
    const auto envCol    = pride ? RetroColors::kPrideFlag[5].brighter (0.45f)
                                 : RetroColors::trace.withAlpha (0.55f); // purple
    const auto modCol    = envCol;

    const auto vcf   = filterPanel.getBounds().toFloat();
    const auto vca   = vcaPanel.getBounds().toFloat();
    const auto fx    = fxPanel.getBounds().toFloat();
    const auto scope = scopePanel.getBounds().toFloat();
    const auto envF  = envFPanel.getBounds().toFloat();
    const auto envA  = envAPanel.getBounds().toFloat();
    const auto mtx   = modMatrixPanel.getBounds().toFloat();
    const auto lfo1b = lfo1Panel.getBounds().toFloat();
    const auto lfo2b = lfo2Panel.getBounds().toFloat();

    // The chain as the engine really runs it:
    //   sources -> CRUSH (pre-filter!) -> VCF -> VCA -> PHASER -> FLANGER -> OUT
    // The 16px channel above the FX strip carries the down/up runs.

    const float gapTop = fx.getY();
    const float yLaneA = gapTop - 12.0f;   // sources -> crush
    const float yLaneB = gapTop - 7.0f;    // crush -> VCF / flanger -> OUT
    const float yLaneC = gapTop - 3.0f;    // VCA -> phaser

    const float crushInX   = fx.getX() + 120.0f;
    const float crushOutX  = fx.getX() + 156.0f;
    const float phaserInX  = fx.getX() + fx.getWidth() * 0.42f;
    const float flangerOutX = fx.getX() + fx.getWidth() * 0.88f;

    // ---- leg 1: OSC1/2/3 + NOISE -> CRUSH (left channel, outer lane) ----
    {
        const float busX = vcf.getX() - 23.0f;
        juce::Component* sources[4] = { &osc1, &osc2, &osc3, &noisePanel };
        const float topY = (float) osc1.getBounds().getCentreY();

        strokeTrace (g, chamfered ({ { busX, topY }, { busX, yLaneA },
                                     { crushInX, yLaneA }, { crushInX, gapTop } }),
                     srcCol, 3.2f);
        juce::Path stubs;
        for (auto* s : sources)
        {
            const float cy = (float) s->getBounds().getCentreY();
            stubs.startNewSubPath ((float) s->getRight(), cy);
            stubs.lineTo (busX, cy);
        }
        strokeTrace (g, stubs, srcCol, 2.0f);
        for (auto* s : sources)
        {
            const float cy = (float) s->getBounds().getCentreY();
            solderPad (g, { (float) s->getRight(), cy }, srcCol);
            if (cy > topY + 1.0f)
                via (g, { busX, cy }, srcCol);
        }
        arrowInto (g, { crushInX, gapTop }, 2, srcCol);
    }

    // ---- leg 2: CRUSH -> VCF (left channel, inner lane) ----
    {
        const float busX = vcf.getX() - 13.0f;
        const float vcfInY = vcf.getY() + 60.0f;
        strokeTrace (g, chamfered ({ { crushOutX, gapTop }, { crushOutX, yLaneB },
                                     { busX, yLaneB }, { busX, vcfInY },
                                     { vcf.getX(), vcfInY } }),
                     vcfCol, 3.2f);
        solderPad (g, { crushOutX, gapTop }, vcfCol);
        arrowInto (g, { vcf.getX(), vcfInY }, 0, vcfCol);
    }

    // ---- leg 3: VCF -> VCA (left channel, panel-hugging lane) ----
    {
        const float laneX = vcf.getX() - 4.0f;
        const float outY = vcf.getBottom() - 18.0f;
        const float inY  = vca.getCentreY();
        strokeTrace (g, chamfered ({ { vcf.getX(), outY }, { laneX, outY },
                                     { laneX, inY }, { vca.getX(), inY } }),
                     vcaCol, 3.2f);
        solderPad (g, { vcf.getX(), outY }, vcaCol);
        arrowInto (g, { vca.getX(), inY }, 0, vcaCol);
    }

    // ---- leg 4: VCA -> PHASER ----
    {
        const float x = vca.getCentreX();
        strokeTrace (g, chamfered ({ { x, vca.getBottom() }, { x, yLaneC },
                                     { phaserInX, yLaneC }, { phaserInX, gapTop } }),
                     phaseCol, 3.0f);
        solderPad (g, { x, vca.getBottom() }, phaseCol);
        arrowInto (g, { phaserInX, gapTop }, 2, phaseCol);
    }

    // ---- leg 5: FLANGER -> OUT (scope window monitors the output) ----
    {
        const float x = scope.getCentreX();
        strokeTrace (g, chamfered ({ { flangerOutX, gapTop }, { flangerOutX, yLaneB },
                                     { x, yLaneB }, { x, scope.getBottom() } }),
                     outCol, 3.0f);
        solderPad (g, { flangerOutX, gapTop }, outCol);
        arrowInto (g, { x, scope.getBottom() }, 3, outCol);
        g.setColour (outCol);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("OUT", (int) x + 6, (int) scope.getBottom() - 4, 26, 10,
                    juce::Justification::centredLeft);
    }

    // ---- env -> target connectors (dashed, through the panel gaps) ----
    {
        juce::Path p;
        const float fx1 = envF.getX() + 56.0f;          // ENV F drives the VCF
        p.startNewSubPath (fx1, envF.getY());
        p.lineTo (fx1, vcf.getBottom());
        const float ax = vca.getCentreX();              // ENV A drives the VCA
        p.startNewSubPath (ax, envA.getBottom());
        p.lineTo (ax, vca.getY());
        strokeTrace (g, p, envCol, 1.4f, true);
        solderPad (g, { fx1, envF.getY() }, envCol);
        solderPad (g, { ax, envA.getBottom() }, envCol);
        arrowInto (g, { fx1, vcf.getBottom() }, 3, envCol);
        arrowInto (g, { ax, vca.getY() }, 2, envCol);
    }

    // ---- modulation web (dashed): envs + matrix -> VCF, LFOs -> matrix ----
    {
        const float busX = vcf.getRight() + 15.0f;
        const float vcfInY = vcf.getBottom() - 24.0f;

        // chamfered bus: matrix output up the channel and into the VCF
        strokeTrace (g, chamfered ({ { mtx.getX(), mtx.getCentreY() },
                                     { busX, mtx.getCentreY() },
                                     { busX, vcfInY },
                                     { vcf.getRight(), vcfInY } }),
                     modCol, 1.4f, true);

        // env sources joining the bus + LFOs dropping into the matrix
        juce::Path p;
        p.startNewSubPath (envF.getRight(), envF.getCentreY());
        p.lineTo (busX, envF.getCentreY());
        p.startNewSubPath (envA.getRight(), envA.getCentreY());
        p.lineTo (busX, envA.getCentreY());
        p.startNewSubPath (lfo1b.getCentreX(), lfo1b.getBottom());
        p.lineTo (lfo1b.getCentreX(), mtx.getY());
        p.startNewSubPath (lfo2b.getCentreX(), lfo2b.getBottom());
        p.lineTo (lfo2b.getCentreX(), mtx.getY());
        strokeTrace (g, p, modCol, 1.4f, true);

        solderPad (g, { envF.getRight(), envF.getCentreY() }, modCol);
        solderPad (g, { envA.getRight(), envA.getCentreY() }, modCol);
        solderPad (g, { mtx.getX(), mtx.getCentreY() }, modCol);
        solderPad (g, { lfo1b.getCentreX(), lfo1b.getBottom() }, modCol);
        solderPad (g, { lfo2b.getCentreX(), lfo2b.getBottom() }, modCol);
        via (g, { busX, envF.getCentreY() }, modCol);
        via (g, { busX, envA.getCentreY() }, modCol);
        arrowInto (g, { vcf.getRight(), vcfInY }, 1, modCol);
        arrowInto (g, { lfo1b.getCentreX(), mtx.getY() }, 2, modCol);
        arrowInto (g, { lfo2b.getCentreX(), mtx.getY() }, 2, modCol);
    }
}

void RetroForgeEditor::resized()
{
    themeEditor.setBounds (getLocalBounds());

    auto b = getLocalBounds();
    header.setBounds (b.removeFromTop (46));
    b.reduce (8, 8);
    constexpr int gap = 6;        // vertical gap within the right column
    constexpr int midGap = 28;    // generous gaps in the compacted mid column
    constexpr int leftGap = 26;   // ... and between the oscillator strips
    constexpr int channel = 30;   // horizontal trace channels between columns

    fxPanel.setBounds (b.removeFromBottom (104));
    b.removeFromBottom (16);   // routing channel above the FX strip

    // ---- left column: oscillators + noise (compact, airy gaps) ----
    auto left = b.removeFromLeft (316);
    osc1.setBounds (left.removeFromTop (156));
    left.removeFromTop (leftGap);
    osc2.setBounds (left.removeFromTop (156));
    left.removeFromTop (leftGap);
    osc3.setBounds (left.removeFromTop (156));
    left.removeFromTop (leftGap);
    noisePanel.setBounds (left);

    b.removeFromLeft (channel);

    // ---- middle column: filter, envelopes, vca + scope (compacted) ----
    auto mid = b.removeFromLeft (400);
    filterPanel.setBounds (mid.removeFromTop (160));
    mid.removeFromTop (midGap);
    envFPanel.setBounds (mid.removeFromTop (152));
    mid.removeFromTop (midGap);
    envAPanel.setBounds (mid.removeFromTop (152));
    mid.removeFromTop (midGap);
    vcaPanel.setBounds (mid.removeFromLeft (168));
    mid.removeFromLeft (gap);
    scopePanel.setBounds (mid);

    b.removeFromLeft (channel);

    // ---- right column: pitch/voices, LFOs, matrix, trigger, generate ----
    auto right = b;
    pitchPanel.setBounds (right.removeFromTop (172));
    right.removeFromTop (gap);
    auto lfoRow = right.removeFromTop (116);
    lfo1Panel.setBounds (lfoRow.removeFromLeft ((lfoRow.getWidth() - gap) / 2));
    lfoRow.removeFromLeft (gap);
    lfo2Panel.setBounds (lfoRow);
    right.removeFromTop (gap);
    modMatrixPanel.setBounds (right.removeFromTop (160));
    right.removeFromTop (gap);
    triggerPanel.setBounds (right.removeFromTop (120));
    right.removeFromTop (gap);
    randomizerPanel.setBounds (right);
}
