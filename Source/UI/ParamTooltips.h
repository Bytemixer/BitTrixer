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

#include "../Params.h"

// ============================================================================
//  ParamTooltips - short one-line hover hints keyed by parameter id. The shared
//  control widgets (PanelCommon) call lookup() so every knob/switch/combo picks
//  up its hint automatically. An empty string means "no tooltip".
// ============================================================================

namespace ParamTooltips
{
    inline juce::String lookup (const juce::String& id)
    {
        namespace P = Params::id;
        auto ends = [&id] (const char* s) { return id.endsWith (s); };

        // ---- oscillators (osc1_/osc2_) and the FM voice transpose (osc3_) ----
        const bool fm = id.startsWith ("osc3_");
        if (ends ("_pitch")) return fm ? "FM voice coarse tuning (semitones)"
                                       : "Oscillator coarse tuning (semitones)";
        if (ends ("_fine"))  return fm ? "FM voice fine tuning (cents)"
                                       : "Oscillator fine tuning (cents)";
        if (ends ("_pwm"))   return "Pulse width of the square wave";
        if (ends ("_fold"))  return "Wavefolder adds harmonics by folding the wave";
        if (id == "osc3_level") return "FM voice output level";
        if (ends ("_level") && id.startsWith ("osc")) return "This oscillator's level in the mix";
        if (ends ("_sync"))  return "Hard-sync this oscillator to OSC 1";

        // ---- noise ----
        if (id == P::noiseColor) return "Noise tone, from white to pink (or the LFSR clock)";
        if (id == P::noiseLevel) return "Noise level in the mix";

        // ---- pitch / base ----
        if (id == P::baseFreq) return "Base pitch of the sound";
        if (id == P::pj1Amt || id == P::pj2Amt)   return "Pitch jump: an sfxr-style arpeggio step in semitones";
        if (id == P::pj1Time || id == P::pj2Time) return "When the pitch jump fires";

        // ---- filter ----
        if (id == P::lpfCutoff) return "Low-pass cutoff (overall brightness)";
        if (id == P::lpfRes)    return "Resonance: emphasis at the cutoff";
        if (id == P::lpfEnv)    return "How much the filter envelope moves the cutoff";
        if (id == P::hpfCutoff) return "High-pass cutoff; removes the low end";

        // ---- envelopes ----
        if (ends ("_attack"))  return "Attack: how fast it rises";
        if (ends ("_decay"))   return "Decay: fall to the sustain level";
        if (ends ("_sustain")) return "Sustain: level held while the note is on";
        if (ends ("_release")) return "Release: fade after note-off";
        if (ends ("_curve"))   return "Segment shape, from exponential to linear to logarithmic";
        if (ends ("_invert"))  return "Flip the envelope upside-down";

        // ---- amp / output ----
        if (id == P::vcaDrive)  return "Drive: saturation and grit at the output";
        if (id == P::comp)      return "Compression: density and punch";
        if (id == P::masterVol) return "Master output level";
        if (id == P::outRate)   return "Output sample rate (lo-fi decimation)";

        // ---- voicing / trigger ----
        if (id == P::uniVoices)  return "Unison: number of stacked detuned voices";
        if (id == P::uniDetune)  return "Unison detune spread (cents)";
        if (id == P::uniSpread)  return "Unison stereo spread";
        if (id == P::gateTime)   return "Auto note length for one-shot triggers";
        if (id == P::retrigRate) return "Re-strike rate for stutter or arpeggio";
        if (id == P::loopRate)   return "Time between auto-retriggers";

        // ---- LFO / step sequencer ----
        if (ends ("lfo1_rate")) return "LFO 1 speed (Hz)";
        if (ends ("lfo2_rate")) return "Step sequencer advance rate (Hz)";
        if (ends ("_delay"))    return "Fade-in delay before the LFO starts";
        if (id == P::stepCount) return "Number of steps in the sequence";
        if (id == P::stepGlide) return "Glide between steps (0 = hard steps)";
        if (id == P::stepSkew)  return "Stagger the step timing (swing)";

        // ---- FX ----
        if (id == P::crushBits) return "Bit depth: lower is crunchier";
        if (id == P::crushDown) return "Sample-rate divider: lower-fi";
        if (id == P::phaseRate || id == P::flangeRate)   return "Sweep rate (Hz)";
        if (id == P::phaseDepth || id == P::flangeDepth) return "Sweep depth";
        if (id == P::phaseFb || id == P::flangeFb || id == P::delayFb) return "Feedback amount";
        if (id == P::ringFreq)  return "Ring-modulator carrier frequency";
        if (id == P::tremRate)  return "Tremolo speed (Hz)";
        if (id == P::tremDepth) return "Tremolo depth";
        if (id == P::formVowel) return "Vowel that morphs the formant filter";
        if (id == P::formReso)  return "Formant resonance and sharpness";
        if (id == P::delayTime) return "Echo time";
        if (ends ("_mix"))      return "Wet / dry balance";

        // ---- FM ----
        if (id == P::fmAlgo)        return "FM algorithm: which operators modulate which";
        if (id == P::fmFeedback)    return "Operator-1 feedback: brightness, then noise";
        if (ends ("_ratio"))        return "Operator frequency ratio (harmonic of the note)";
        if (id.startsWith ("fmop")) return "Operator level: carrier gain or modulation depth";

        // ---- mod matrix ----
        if (ends ("_src"))   return "Modulation source";
        if (ends ("_dest"))  return "Modulation destination";
        if (ends ("_depth")) return "Modulation amount (bipolar)";

        return {};
    }
}
