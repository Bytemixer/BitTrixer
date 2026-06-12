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
}

void RetroForgeEditor::drawSignalTraces (juce::Graphics& g)
{
    const auto audioCol = RetroColors::trace;
    const auto modCol   = RetroColors::trace.withAlpha (0.55f);

    const auto vcf   = filterPanel.getBounds().toFloat();
    const auto vca   = vcaPanel.getBounds().toFloat();
    const auto fx    = fxPanel.getBounds().toFloat();
    const auto scope = scopePanel.getBounds().toFloat();
    const auto envF  = envFPanel.getBounds().toFloat();
    const auto envA  = envAPanel.getBounds().toFloat();
    const auto mtx   = modMatrixPanel.getBounds().toFloat();
    const auto lfo1b = lfo1Panel.getBounds().toFloat();
    const auto lfo2b = lfo2Panel.getBounds().toFloat();

    // ---- source bus: OSC1/2/3 + NOISE -> VCF (left channel, lane 1) ----
    {
        const float busX = vcf.getX() - 9.0f;
        const float vcfInY = vcf.getY() + 60.0f;
        juce::Path p;

        juce::Component* sources[4] = { &osc1, &osc2, &osc3, &noisePanel };
        float topY = 1.0e9f, botY = -1.0e9f;
        for (auto* s : sources)
        {
            const float cy = (float) s->getBounds().getCentreY();
            p.startNewSubPath ((float) s->getRight(), cy);
            p.lineTo (busX, cy);
            topY = juce::jmin (topY, cy);
            botY = juce::jmax (botY, cy);
        }
        p.startNewSubPath (busX, juce::jmin (topY, vcfInY));
        p.lineTo (busX, botY);
        p.startNewSubPath (busX, vcfInY);
        p.lineTo (vcf.getX(), vcfInY);

        strokeTrace (g, p, audioCol, 3.0f);
        for (auto* s : sources)
            solderPad (g, { (float) s->getRight(), (float) s->getBounds().getCentreY() }, audioCol);
        arrowInto (g, { vcf.getX(), vcfInY }, 0, audioCol);
    }

    // ---- downstream: VCF -> VCA (left channel, lane 2) ----
    {
        const float laneX = vcf.getX() - 4.0f;
        const float outY = vcf.getBottom() - 18.0f;
        const float inY  = vca.getCentreY();
        juce::Path p;
        p.startNewSubPath (vcf.getX(), outY);
        p.lineTo (laneX, outY);
        p.lineTo (laneX, inY);
        p.lineTo (vca.getX(), inY);
        strokeTrace (g, p, audioCol, 3.0f);
        solderPad (g, { vcf.getX(), outY }, audioCol);
        arrowInto (g, { vca.getX(), inY }, 0, audioCol);
    }

    // ---- VCA -> FX (vertical gap) ----
    {
        const float x = vca.getCentreX();
        juce::Path p;
        p.startNewSubPath (x, vca.getBottom());
        p.lineTo (x, fx.getY());
        strokeTrace (g, p, audioCol, 3.0f);
        solderPad (g, { x, vca.getBottom() }, audioCol);
        arrowInto (g, { x, fx.getY() }, 2, audioCol);
    }

    // ---- FX -> OUT (scope window monitors the output) ----
    {
        const float x = scope.getCentreX();
        juce::Path p;
        p.startNewSubPath (x, fx.getY());
        p.lineTo (x, scope.getBottom());
        strokeTrace (g, p, audioCol, 3.0f);
        solderPad (g, { x, fx.getY() }, audioCol);
        arrowInto (g, { x, scope.getBottom() }, 3, audioCol);
        g.setColour (audioCol);
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        g.drawText ("OUT", (int) x + 6, (int) scope.getBottom() - 4, 26, 10,
                    juce::Justification::centredLeft);
    }

    // ---- modulation web (dashed): envs + matrix -> VCF, LFOs -> matrix ----
    {
        const float busX = vcf.getRight() + 7.0f;
        const float vcfInY = vcf.getBottom() - 24.0f;
        juce::Path p;
        p.startNewSubPath (busX, vcfInY);
        p.lineTo (busX, mtx.getCentreY());
        p.startNewSubPath (vcf.getRight(), vcfInY);
        p.lineTo (busX, vcfInY);
        p.startNewSubPath (envF.getRight(), envF.getCentreY());
        p.lineTo (busX, envF.getCentreY());
        p.startNewSubPath (envA.getRight(), envA.getCentreY());
        p.lineTo (busX, envA.getCentreY());
        p.startNewSubPath (busX, mtx.getCentreY());
        p.lineTo (mtx.getX(), mtx.getCentreY());

        // LFOs drop into the matrix
        p.startNewSubPath (lfo1b.getCentreX(), lfo1b.getBottom());
        p.lineTo (lfo1b.getCentreX(), mtx.getY());
        p.startNewSubPath (lfo2b.getCentreX(), lfo2b.getBottom());
        p.lineTo (lfo2b.getCentreX(), mtx.getY());

        strokeTrace (g, p, modCol, 1.6f, true);
        solderPad (g, { envF.getRight(), envF.getCentreY() }, modCol);
        solderPad (g, { envA.getRight(), envA.getCentreY() }, modCol);
        solderPad (g, { mtx.getX(), mtx.getCentreY() }, modCol);
        solderPad (g, { lfo1b.getCentreX(), lfo1b.getBottom() }, modCol);
        solderPad (g, { lfo2b.getCentreX(), lfo2b.getBottom() }, modCol);
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
    constexpr int gap = 6;        // vertical gap within columns
    constexpr int channel = 14;   // horizontal trace channels between columns

    fxPanel.setBounds (b.removeFromBottom (104));
    b.removeFromBottom (gap);

    // ---- left column: oscillators + noise ----
    auto left = b.removeFromLeft (324);
    osc1.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    osc2.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    osc3.setBounds (left.removeFromTop (168));
    left.removeFromTop (gap);
    noisePanel.setBounds (left);

    b.removeFromLeft (channel);

    // ---- middle column: filter, envelopes, vca + scope ----
    auto mid = b.removeFromLeft (424);
    filterPanel.setBounds (mid.removeFromTop (180));
    mid.removeFromTop (gap);
    envFPanel.setBounds (mid.removeFromTop (164));
    mid.removeFromTop (gap);
    envAPanel.setBounds (mid.removeFromTop (164));
    mid.removeFromTop (gap);
    vcaPanel.setBounds (mid.removeFromLeft (178));
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
