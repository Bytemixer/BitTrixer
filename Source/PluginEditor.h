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

#include "PluginProcessor.h"
#include "Randomizer.h"
#include "PresetManager.h"
#include "WavExporter.h"
#include "UI/RetroLookAndFeel.h"
#include "UI/HeaderBar.h"
#include "UI/OscPanel.h"
#include "UI/NoisePanel.h"
#include "UI/PitchPanel.h"
#include "UI/FilterPanel.h"
#include "UI/EnvPanel.h"
#include "UI/VcaPanel.h"
#include "UI/LfoPanel.h"
#include "UI/ModMatrixPanel.h"
#include "UI/TriggerPanel.h"
#include "UI/RandomizerPanel.h"

// ============================================================================
//  RetroForgeEditor
//  The front panel. Arranges the per-section panel components; all controls
//  live inside the panels (Source/UI/*), never directly in here.
// ============================================================================

class RetroForgeEditor : public juce::AudioProcessorEditor
{
public:
    explicit RetroForgeEditor (RetroForgeProcessor&);
    ~RetroForgeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void previewSound();

    RetroForgeProcessor& proc;

    RetroLookAndFeel lookAndFeel;
    Randomizer randomizer;
    PresetManager presetManager;
    WavExporter wavExporter;

    HeaderBar header;
    OscPanel osc1, osc2, osc3;
    NoisePanel noisePanel;
    FilterPanel filterPanel;
    EnvPanel envFPanel, envAPanel;
    VcaPanel vcaPanel;
    PitchPanel pitchPanel;
    LfoPanel lfo1Panel, lfo2Panel;
    ModMatrixPanel modMatrixPanel;
    TriggerPanel triggerPanel;
    RandomizerPanel randomizerPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RetroForgeEditor)
};
