/*  This file is part of the BitTrixer audio plugin.
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
//  WavExporter — offline-renders a patch with a private engine instance (one
//  timed gate + tail) and writes it as a stereo WAV at the host rate, with the
//  lo-fi stage's sample-and-hold steps included, in the patch's bit depth
//  (8 or 16). Each render owns its engine, so any thread may render.
// ============================================================================

class WavExporter
{
public:
    explicit WavExporter (double sampleRateToUse = 44100.0)
        : sampleRate (sampleRateToUse) {}

    void setSampleRate (double sr) { sampleRate = sr > 8000.0 ? sr : 44100.0; }

    // a render at the host rate with the patch's bit depth
    struct FileAudio
    {
        juce::AudioBuffer<float> buffer;
        double rate = 44100.0;
        int bits = 16;
    };

    FileAudio renderForFile (const Params::Patch& patch)
    {
        FileAudio f;
        f.buffer = renderPatch (patch);
        f.rate = sampleRate;
        f.bits = patch.out8bit ? 8 : 16;
        return f;
    }

    static bool writeWav (const FileAudio& f, const juce::File& file)
    {
        if (f.buffer.getNumSamples() == 0)
            return false;

        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
        if (stream == nullptr)
            return false;

        auto writer = wav.createWriterFor (stream,
            juce::AudioFormatWriterOptions{}
                .withSampleRate (f.rate)
                .withNumChannels (f.buffer.getNumChannels())
                .withBitsPerSample (f.bits));
        if (writer == nullptr)
            return false;

        return writer->writeFromAudioSampleBuffer (f.buffer, 0, f.buffer.getNumSamples());
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
};
