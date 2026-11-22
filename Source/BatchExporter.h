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

#include <atomic>
#include <memory>
#include <vector>
#include "Params.h"
#include "ShadowParams.h"
#include "Variation.h"
#include "Loudness.h"
#include "WavExporter.h"

// ============================================================================
//  BatchExport — renders one WAV or N VARIATE siblings of a sound on a
//  background thread, from a ShadowParams copy, then level-matches and
//  writes them. Variant i draws its nudges from Random (variantSeed (seed, i)),
//  so a seed re-renders the same batch.
// ============================================================================

namespace BatchExport
{
    enum class LevelMode { Off = 0, Peak, Loudness, Target };

    inline constexpr float kCeilingDbTP = -1.0f;
    inline constexpr int   kMaxCount    = 64;

    struct Settings
    {
        juce::String name;
        juce::File   folder;
        bool         subfolder = false;
        int          count = 1;
        float        amount = 0.15f;
        bool         keepOriginal = true;
        juce::int64  seed = 1;
        LevelMode    level = LevelMode::Off;
        float        targetLufs = -16.0f;
    };

    struct Report
    {
        juce::File file;
        float lufs = Loudness::kSilenceDb;
        float truePeakDb = Loudness::kSilenceDb;
        float gainDb = 0.0f;
        bool  limited = false;   // gain held back by the true-peak ceiling
        bool  written = false;
    };

    inline juce::File outputDir (const Settings& s)
    {
        return s.subfolder ? s.folder.getChildFile (s.name) : s.folder;
    }

    // count 1 -> Name.wav; count N -> Name_0.wav .. Name_{N-1}.wav
    inline juce::Array<juce::File> outputFiles (const Settings& s)
    {
        juce::Array<juce::File> files;
        const auto dir = outputDir (s);
        if (s.count <= 1)
            files.add (dir.getChildFile (s.name + ".wav"));
        else
            for (int i = 0; i < s.count; ++i)
                files.add (dir.getChildFile (s.name + "_" + juce::String (i) + ".wav"));
        return files;
    }

    // splitmix64 over (seed, index): batches of neighbouring seeds never overlap
    inline juce::int64 variantSeed (juce::int64 seed, int index)
    {
        auto z = (juce::uint64) seed + 0x9E3779B97F4A7C15ull * (juce::uint64) (index + 1);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return (juce::int64) (z ^ (z >> 31));
    }

    // gain in dB for one sound, and whether the ceiling held it back
    inline float matchGain (LevelMode mode, const Loudness::Result& m,
                            const Loudness::Result& ref, float targetLufs, bool& limited)
    {
        limited = false;
        if (mode == LevelMode::Off || m.lufs <= Loudness::kSilenceDb)
            return 0.0f;

        // the reference as it is written: held at the ceiling itself
        const float refTrim = juce::jmin (0.0f, kCeilingDbTP - ref.truePeakDb);
        float g = 0.0f;
        switch (mode)
        {
            case LevelMode::Peak:     g = (ref.truePeakDb + refTrim) - m.truePeakDb; break;
            case LevelMode::Loudness: g = (ref.lufs + refTrim) - m.lufs;             break;
            case LevelMode::Target:   g = targetLufs - m.lufs;                       break;
            case LevelMode::Off:      break;
        }
        if (m.truePeakDb + g > kCeilingDbTP)
        {
            g = kCeilingDbTP - m.truePeakDb;
            limited = true;
        }
        return g;
    }

    class Job : public juce::Thread
    {
    public:
        Job (const Settings& s, std::unique_ptr<ShadowParams> baseParams, double sampleRate)
            : juce::Thread ("BitTrixer batch export"),
              settings (s), base (std::move (baseParams)), rate (sampleRate) {}

        ~Job() override { stopThread (10000); }

        float progress() const noexcept { return done.load() / (float) juce::jmax (1, settings.count); }
        bool  finished() const noexcept { return complete.load(); }

        // valid once finished() is true
        const std::vector<Report>& reports() const noexcept { return results; }
        bool  folderFailed() const noexcept { return dirFailed; }
        const Loudness::Result& reference() const noexcept { return ref; }

        void run() override
        {
            const auto files = outputFiles (settings);
            const auto dir = outputDir (settings);
            dirFailed = ! dir.createDirectory().wasOk();

            ShadowParams work (*base);
            Params::Cache cache ([&work] (const juce::String& pid) { return work.raw (pid); });
            WavExporter exporter (rate);

            const auto original = exporter.renderForFile (cache.read());
            ref = Loudness::measure (original.buffer, original.rate);
            const auto targets = Variation::targets (true);

            for (int i = 0; i < settings.count && ! dirFailed; ++i)
            {
                if (threadShouldExit())
                    break;

                const bool unchanged = settings.count <= 1 || (settings.keepOriginal && i == 0);
                WavExporter::FileAudio audio;
                Loudness::Result m;
                if (unchanged)
                {
                    audio = original;
                    m = ref;
                }
                else
                {
                    work.copyValuesFrom (*base);
                    juce::Random rng (variantSeed (settings.seed, i));
                    Variation::apply (targets, Variation::kVariateScale, settings.amount, rng,
                        [&work] (const juce::String& pid, float& v) { return work.getNormalised (pid, v); },
                        [&work] (const juce::String& pid, float v)  { work.setNormalised (pid, v); });
                    audio = exporter.renderForFile (cache.read());
                    m = Loudness::measure (audio.buffer, audio.rate);
                }

                Report r;
                r.file = files[i];
                r.gainDb = matchGain (settings.level, m, ref, settings.targetLufs, r.limited);
                if (r.gainDb != 0.0f)
                    audio.buffer.applyGain (juce::Decibels::decibelsToGain (r.gainDb));
                r.lufs = m.lufs <= Loudness::kSilenceDb ? m.lufs : m.lufs + r.gainDb;
                r.truePeakDb = m.truePeakDb <= Loudness::kSilenceDb ? m.truePeakDb : m.truePeakDb + r.gainDb;
                r.written = WavExporter::writeWav (audio, r.file);
                results.push_back (r);
                done.store (i + 1);
            }
            complete.store (true);
        }

    private:
        const Settings settings;
        const std::unique_ptr<ShadowParams> base;
        const double rate;

        std::vector<Report> results;
        Loudness::Result ref;
        bool dirFailed = false;
        std::atomic<int>  done { 0 };
        std::atomic<bool> complete { false };
    };
}
