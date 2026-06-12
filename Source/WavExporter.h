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

#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "DSP/SynthEngine.h"

// ============================================================================
//  WavExporter — offline-renders the current patch with a private engine
//  instance (one timed gate + tail) and writes a 16-bit stereo WAV.
//  Designed for the standalone "design a sound, drop it into the game"
//  workflow. Message-thread only; the render itself takes milliseconds.
// ============================================================================

class WavExporter
{
public:
    explicit WavExporter (double sampleRateToUse = 44100.0)
        : sampleRate (sampleRateToUse) {}

    void setSampleRate (double sr) { sampleRate = sr > 8000.0 ? sr : 44100.0; }

    void exportAsync (const Params::Patch& patch)
    {
        pendingPatch = patch;
        chooser = std::make_unique<juce::FileChooser> (
            "Export WAV",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile ("retroforge_sfx.wav"),
            "*.wav");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                renderToFile (pendingPatch, file.withFileExtension ("wav"));
            });
    }

    bool renderToFile (const Params::Patch& patch, const juce::File& file)
    {
        const auto buffer = renderPatch (patch);
        if (buffer.getNumSamples() == 0)
            return false;

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        auto writer = wav.createWriterFor (stream,
            juce::AudioFormatWriterOptions{}
                .withSampleRate (sampleRate)
                .withNumChannels (buffer.getNumChannels())
                .withBitsPerSample (16));
        if (writer == nullptr)
            return false;

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return true;
    }

    juce::AudioBuffer<float> renderPatch (const Params::Patch& patchIn,
                                          double maxSeconds = 12.0)
    {
        constexpr int    kBlock = 512;
        const int        maxSamples = (int) (maxSeconds * sampleRate);

        // offline determinism: one shot only, no loop retriggers, no variate
        Params::Patch patch = patchIn;
        patch.loopOn = false;
        patch.autoVarOn = false;

        SynthEngine engine;
        engine.setPatch (patch);
        engine.prepare (sampleRate, kBlock);
        engine.setPatch (patch);
        engine.oneShot();

        juce::AudioBuffer<float> out (2, maxSamples);
        int total = 0;
        while (total < maxSamples)
        {
            const int n = juce::jmin (kBlock, maxSamples - total);
            engine.render (out.getWritePointer (0, total),
                           out.getWritePointer (1, total), n);
            total += n;
            if (! engine.anyActive())
                break;
        }

        // trim trailing silence below -80 dBFS, keep a 50 ms cushion
        constexpr float kFloor = 0.0001f;
        int last = total - 1;
        while (last > 0
               && std::fabs (out.getSample (0, last)) < kFloor
               && std::fabs (out.getSample (1, last)) < kFloor)
            --last;
        const int finalLen = juce::jmin (total, last + 1 + (int) (0.05 * sampleRate));

        out.setSize (2, finalLen, true, true, true);
        return out;
    }

private:
    double sampleRate = 44100.0;
    Params::Patch pendingPatch;
    std::unique_ptr<juce::FileChooser> chooser;
};
