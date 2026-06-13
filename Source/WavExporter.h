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

#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"
#include "DSP/SynthEngine.h"

// ============================================================================
//  WavExporter — offline-renders the current patch with a private engine
//  instance (one timed gate + tail) and writes a stereo WAV at the patch's
//  selected output rate (8/11/22 kHz ... or host) and bit depth (8 or 16).
//  The render runs at the host rate so the sample-and-hold grit matches the
//  live preview; the result is then decimated down to the chosen rate, giving
//  a small, correctly-tagged file (jsfxr-style) that still sounds identical.
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
        auto buffer = renderPatch (patch);
        if (buffer.getNumSamples() == 0)
            return false;

        // Match the file format to the patch's lo-fi output settings so the
        // exported file IS the chosen rate / bit depth -- small and correctly
        // tagged -- instead of a fat host-rate / 16-bit container. The render
        // already baked the sample-and-hold grit in at host rate; decimating
        // it back down recovers those exact staircase values, so the small
        // file still sounds like the live preview.
        double fileRate = sampleRate;
        if (patch.outRateHz > 0.0f && (double) patch.outRateHz < sampleRate - 1.0)
        {
            buffer   = decimateToRate (buffer, sampleRate, (double) patch.outRateHz);
            fileRate = (double) patch.outRateHz;
        }
        const int bitsPerSample = patch.out8bit ? 8 : 16;

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        auto writer = wav.createWriterFor (stream,
            juce::AudioFormatWriterOptions{}
                .withSampleRate (fileRate)
                .withNumChannels (buffer.getNumChannels())
                .withBitsPerSample (bitsPerSample));
        if (writer == nullptr)
            return false;

        writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
        return true;
    }

    // Down-sample a host-rate buffer to targetRate by picking the sample-and-hold
    // plateau values, replaying the same fractional stride LofiStage used to
    // create them (decimStep = hostRate / targetRate). Because the input is
    // already piecewise-constant on each plateau, this lands on the true
    // target-rate samples -- a genuine low-rate buffer, like a jsfxr export.
    static juce::AudioBuffer<float> decimateToRate (const juce::AudioBuffer<float>& src,
                                                    double hostRate, double targetRate)
    {
        const double step  = hostRate / targetRate;     // mirrors LofiStage decimStep
        const int    nIn   = src.getNumSamples();
        const int    chans = src.getNumChannels();
        const int    nOutMax = (int) std::ceil ((double) nIn / step) + 1;

        juce::AudioBuffer<float> out (chans, nOutMax);
        out.clear();

        int    outIdx = 0;
        double count  = 0.0;
        for (int i = 0; i < nIn && outIdx < nOutMax; ++i)
        {
            if (count <= 0.0)
            {
                count += step;
                for (int c = 0; c < chans; ++c)
                    out.setSample (c, outIdx, src.getSample (c, i));
                ++outIdx;
            }
            count -= 1.0;
        }

        out.setSize (chans, outIdx, true, true, true);
        return out;
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
