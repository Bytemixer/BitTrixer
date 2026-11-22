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
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>

// ============================================================================
//  Loudness — measures a rendered sound for level matching.
//  * lufs: ITU-R BS.1770 K-weighted loudness, ungated, over the loudest
//    100 ms window (the whole sound if it is shorter). Channels summed.
//  * truePeakDb: peak of the signal upsampled 4x (BS.1770 Annex 2 style).
//  Coefficients are computed for any sample rate, as libebur128 does.
// ============================================================================

namespace Loudness
{
    inline constexpr double kWindowSec = 0.1;
    inline constexpr float  kSilenceDb = -150.0f;

    struct Result
    {
        float lufs = kSilenceDb;
        float truePeakDb = kSilenceDb;
    };

    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        double tick (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // BS.1770 stage 1 (high shelf) and stage 2 (RLB high-pass) for rate fs
    inline void kWeighting (double fs, Biquad& shelf, Biquad& highPass)
    {
        const double pi = juce::MathConstants<double>::pi;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K  = std::tan (pi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K  = std::tan (pi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            highPass.b0 = 1.0;  highPass.b1 = -2.0;  highPass.b2 = 1.0;
            highPass.a1 = 2.0 * (K * K - 1.0) / a0;
            highPass.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }

    inline float toDb (double linearPower, double offset = 0.0)
    {
        return linearPower > 0.0 ? (float) (offset + 10.0 * std::log10 (linearPower)) : kSilenceDb;
    }

    // ungated K-weighted loudness of the loudest window, in LUFS
    inline float windowLufs (const juce::AudioBuffer<float>& buf, double fs)
    {
        const int n = buf.getNumSamples();
        if (n == 0)
            return kSilenceDb;

        std::vector<double> power ((size_t) n, 0.0);
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            Biquad shelf, hp;
            kWeighting (fs, shelf, hp);
            const float* x = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
            {
                const double y = hp.tick (shelf.tick ((double) x[i]));
                power[(size_t) i] += y * y;
            }
        }

        const int win = juce::jlimit (1, n, (int) std::lround (kWindowSec * fs));
        double sum = 0.0;
        for (int i = 0; i < win; ++i)
            sum += power[(size_t) i];
        double best = sum;
        for (int i = win; i < n; ++i)
        {
            sum += power[(size_t) i] - power[(size_t) (i - win)];
            best = std::max (best, sum);
        }
        return toDb (best / (double) win, -0.691);
    }

    // 4x upsampled peak through a 48-tap Kaiser-windowed sinc, in dBTP
    inline float truePeakDb (const juce::AudioBuffer<float>& buf)
    {
        static constexpr int kPhases = 4, kTaps = 12;   // taps per phase
        static const auto coeffs = []
        {
            std::vector<double> h ((size_t) (kPhases * kTaps));
            const double centre = (kPhases * kTaps - 1) * 0.5, beta = 8.0;
            const double i0b = std::cyl_bessel_i (0.0, beta);
            for (int k = 0; k < kPhases * kTaps; ++k)
            {
                const double t = (k - centre) / kPhases;
                const double sinc = std::abs (t) < 1e-12 ? 1.0
                                  : std::sin (juce::MathConstants<double>::pi * t)
                                      / (juce::MathConstants<double>::pi * t);
                const double r = (k - centre) / centre;
                const double w = std::cyl_bessel_i (0.0, beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
                h[(size_t) k] = sinc * w;
            }
            // each phase sums to unity gain on DC
            for (int p = 0; p < kPhases; ++p)
            {
                double s = 0.0;
                for (int j = 0; j < kTaps; ++j) s += h[(size_t) (j * kPhases + p)];
                for (int j = 0; j < kTaps; ++j) h[(size_t) (j * kPhases + p)] /= s;
            }
            return h;
        }();

        const int n = buf.getNumSamples();
        double peak = 0.0;
        for (int c = 0; c < buf.getNumChannels(); ++c)
        {
            const float* x = buf.getReadPointer (c);
            for (int i = 0; i < n + kTaps; ++i)
                for (int p = 0; p < kPhases; ++p)
                {
                    double y = 0.0;
                    for (int j = 0; j < kTaps; ++j)
                    {
                        const int idx = i - j;
                        if (idx >= 0 && idx < n)
                            y += coeffs[(size_t) (j * kPhases + p)] * (double) x[idx];
                    }
                    peak = std::max (peak, std::abs (y));
                }
            for (int i = 0; i < n; ++i)
                peak = std::max (peak, (double) std::abs (x[i]));
        }
        return toDb (peak * peak);
    }

    inline Result measure (const juce::AudioBuffer<float>& buf, double fs)
    {
        return { windowLufs (buf, fs), truePeakDb (buf) };
    }
}
