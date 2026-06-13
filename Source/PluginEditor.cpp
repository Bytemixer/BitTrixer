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
      header (p.apvts),
      generatorsPanel (p.apvts),
      filterPanel (p.apvts),
      envelopesPanel (p.apvts),
      vcaPanel (p.apvts),
      pitchPanel (p.apvts),
      lfo1Panel (p.apvts, 1), stepLfoPanel (p.apvts),
      modMatrixPanel (p.apvts),
      triggerPanel (p.apvts),
      scopePanel (p.apvts, [&p] { return p.snapshotPatch(); }),
      fxPanel (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    for (auto* c : std::initializer_list<juce::Component*> {
             &header, &generatorsPanel, &filterPanel,
             &envelopesPanel, &vcaPanel, &pitchPanel,
             &lfo1Panel, &stepLfoPanel, &modMatrixPanel,
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
    // audio path: one colour (or the pride ribbon); control path: a
    // contrasting colour, like the blue-audio / red-control synth diagrams
    const auto srcCol    = leg (0);
    const auto vcfCol    = leg (1);
    const auto vcaCol    = leg (2);
    const auto phaseCol  = leg (3);
    const auto outCol    = leg (4);
    const auto envCol    = pride ? RetroColors::kPrideFlag[5].brighter (0.45f)
                                 : RetroColors::traceCtrl;
    const auto modCol    = envCol;

    const auto vcf   = filterPanel.getBounds().toFloat();
    const auto vca   = vcaPanel.getBounds().toFloat();
    const auto fx    = fxPanel.getBounds().toFloat();
    const auto scope = scopePanel.getBounds().toFloat();
    const auto envF  = envelopesPanel.filterBandBounds().toFloat();
    const auto envA  = envelopesPanel.ampBandBounds().toFloat();
    const auto mtx   = modMatrixPanel.getBounds().toFloat();
    const auto lfo1b = lfo1Panel.getBounds().toFloat();
    const auto lfo2b = stepLfoPanel.getBounds().toFloat();

    // The chain as the engine really runs it:
    //   sources -> CRUSH (pre-filter!) -> VCF -> VCA -> PHASER -> FLANGER -> OUT
    // The 16px channel above the FX strip carries the down/up runs.

    const float gapTop = fx.getY();
    const float yLaneB = gapTop - 7.0f;    // crush -> VCF / flanger -> OUT
    const float yLaneC = gapTop - 3.0f;    // VCA -> phaser

    const float crushInX   = fx.getX() + 120.0f;
    const float crushOutX  = fx.getX() + 156.0f;
    const float phaserInX  = fx.getX() + fx.getWidth() * 0.42f;
    const float flangerOutX = fx.getX() + fx.getWidth() * 0.88f;

    // ---- leg 1: SOUND GENERATORS -> CRUSH (down the open routing lane) ----
    {
        const auto gen = generatorsPanel.getBounds().toFloat();
        const float outX = gen.getCentreX();
        const float laneY = (gen.getBottom() + gapTop) * 0.5f;   // mid routing lane

        // all generators sum and flow out the bottom, run the open lane, and
        // dive into the bitcrusher (which sits BEFORE the filter)
        strokeTrace (g, chamfered ({ { outX, gen.getBottom() }, { outX, laneY },
                                     { crushInX, laneY }, { crushInX, gapTop } }, 12.0f),
                     srcCol, 3.4f);
        arrowInto (g, { crushInX, gapTop }, 2, srcCol);
        g.setColour (srcCol);
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.drawText ("SOURCES", (int) outX + 12, (int) laneY - 12, 70, 11,
                    juce::Justification::centredLeft);
    }

    // ---- leg 2: CRUSH -> VCF (left channel, inner lane) ----
    {
        const float busX = vcf.getX() - 22.0f;        // further left, into the channel
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

    auto modLabel = [&g] (juce::Colour c, juce::Point<float> at, const char* txt,
                          juce::Justification just)
    {
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (8.5f, juce::Font::bold)));
        g.drawText (txt, (int) at.x - 40, (int) at.y - 6, 80, 12, just);
    };

    // ---- the envelopes drive the panels above and below: a single simple
    //      arrow in each gap (ENV F up to the VCF, ENV A down to the VCA),
    //      styled like the LFO -> Mod Matrix arrow ----
    {
        const float cx = envF.getCentreX();
        juce::Path up;
        up.startNewSubPath (cx, envF.getY());
        up.lineTo (cx, vcf.getBottom());
        strokeTrace (g, up, envCol, 1.4f, true);
        arrowInto (g, { cx, vcf.getBottom() }, 3, envCol);

        // aim at the VCA, which sits on the LEFT of the VCA/scope row
        const float ax = vca.getCentreX();
        juce::Path down;
        down.startNewSubPath (ax, envA.getBottom());
        down.lineTo (ax, vca.getY());
        strokeTrace (g, down, envCol, 1.4f, true);
        arrowInto (g, { ax, vca.getY() }, 2, envCol);
    }

    // ---- mod matrix: LFO1/2 + ENV F/A are SOURCES -> matrix; matrix
    //      OUTPUT fans back to the audio path (cutoff/res, pitch, ...) ----
    {
        const float inLaneX  = mtx.getX() - 10.0f;       // sources collect here
        const float outLaneX = vcf.getRight() + 9.0f;    // matrix output rail
        const float matrixInY  = mtx.getY() + 14.0f;
        const float matrixOutY = mtx.getBottom() - 14.0f;
        const float vcfModInY  = vcf.getBottom() - 22.0f;

        // LFO1 / LFO2 drop straight down into the matrix top
        juce::Path lfo;
        lfo.startNewSubPath (lfo1b.getCentreX(), lfo1b.getBottom());
        lfo.lineTo (lfo1b.getCentreX(), mtx.getY());
        lfo.startNewSubPath (lfo2b.getCentreX(), lfo2b.getBottom());
        lfo.lineTo (lfo2b.getCentreX(), mtx.getY());
        strokeTrace (g, lfo, modCol, 1.3f, true);

        // ENV F / ENV A are also selectable matrix sources
        strokeTrace (g, chamfered ({ { envF.getRight(), envF.getCentreY() },
                                     { inLaneX, envF.getCentreY() },
                                     { inLaneX, matrixInY },
                                     { mtx.getX(), matrixInY } }, 5.0f),
                     modCol, 1.3f, true);
        strokeTrace (g, chamfered ({ { envA.getRight(), envA.getCentreY() },
                                     { inLaneX, envA.getCentreY() },
                                     { inLaneX, matrixInY } }, 5.0f),
                     modCol, 1.3f, true);

        // matrix OUTPUT -> back across to the VCF (nearest audio destination)
        strokeTrace (g, chamfered ({ { mtx.getX(), matrixOutY },
                                     { outLaneX, matrixOutY },
                                     { outLaneX, vcfModInY },
                                     { vcf.getRight(), vcfModInY } }, 6.0f),
                     modCol, 1.7f, true);

        solderPad (g, { lfo1b.getCentreX(), lfo1b.getBottom() }, modCol);
        solderPad (g, { lfo2b.getCentreX(), lfo2b.getBottom() }, modCol);
        solderPad (g, { envF.getRight(), envF.getCentreY() }, modCol);
        solderPad (g, { envA.getRight(), envA.getCentreY() }, modCol);
        solderPad (g, { mtx.getX(), matrixOutY }, modCol);
        via (g, { inLaneX, matrixInY }, modCol);
        arrowInto (g, { lfo1b.getCentreX(), mtx.getY() }, 2, modCol);
        arrowInto (g, { lfo2b.getCentreX(), mtx.getY() }, 2, modCol);
        arrowInto (g, { mtx.getX(), matrixInY }, 0, modCol);
        arrowInto (g, { vcf.getRight(), vcfModInY }, 1, modCol);
        modLabel (modCol, { outLaneX, (matrixOutY + vcfModInY) * 0.5f },
                  "MOD", juce::Justification::centredRight);
    }
}

void RetroForgeEditor::resized()
{
    themeEditor.setBounds (getLocalBounds());

    auto b = getLocalBounds();
    header.setBounds (b.removeFromTop (46));
    b.reduce (8, 8);
    constexpr int gap = 6;        // vertical gap within the right column
    constexpr int leftGap = 26;   // between the oscillator strips
    constexpr int channel = 30;   // horizontal trace channels between columns

    fxPanel.setBounds (b.removeFromBottom (104));
    b.removeFromBottom (16);   // routing channel above the FX strip

    // ---- left column: all sound generators in one panel, leaving an open
    //      routing lane below it for the source -> filter-chain traces ----
    juce::ignoreUnused (leftGap);
    {
        auto leftCol = b.removeFromLeft (316);
        leftCol.removeFromBottom (132);          // routing lane (background)
        generatorsPanel.setBounds (leftCol);
    }

    b.removeFromLeft (channel);

    // ---- middle column: filter, both envelopes (one panel), vca + scope.
    //      the inter-panel routing is just a small arrow now, so the gaps
    //      are LFO->matrix sized and the envelopes take the reclaimed room ----
    constexpr int midArrowGap = 10;
    auto mid = b.removeFromLeft (400);
    filterPanel.setBounds (mid.removeFromTop (152));
    mid.removeFromTop (midArrowGap);
    envelopesPanel.setBounds (mid.removeFromTop (376));
    mid.removeFromTop (midArrowGap);
    vcaPanel.setBounds (mid.removeFromLeft (200));
    mid.removeFromLeft (gap);
    scopePanel.setBounds (mid);

    b.removeFromLeft (channel);

    // ---- right column: pitch/voices, LFOs, matrix, trigger, generate ----
    auto right = b;
    pitchPanel.setBounds (right.removeFromTop (172));
    right.removeFromTop (gap);
    auto lfoRow = right.removeFromTop (116);
    lfo1Panel.setBounds (lfoRow.removeFromLeft (150));
    lfoRow.removeFromLeft (gap);
    stepLfoPanel.setBounds (lfoRow);
    right.removeFromTop (gap);
    modMatrixPanel.setBounds (right.removeFromTop (146));
    right.removeFromTop (gap);
    triggerPanel.setBounds (right.removeFromTop (146));
    right.removeFromTop (gap);
    randomizerPanel.setBounds (right);
}
