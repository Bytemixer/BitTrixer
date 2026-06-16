/*  This file is part of the BitTrixer audio plugin.
    Copyright (C) 2026 Bytemixer
    SPDX-License-Identifier: AGPL-3.0-or-later

    This program is free software: you can redistribute it and/or modify it
    under the terms of the GNU Affero General Public License as published by
    the Free Software Foundation, either version 3 of the License, or (at
    your option) any later version. It is distributed WITHOUT ANY WARRANTY;
    see the LICENSE file for details.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

BitTrixerProcessor::BitTrixerProcessor()
    : AudioProcessor (BusesProperties()
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", Params::createLayout()),
      paramCache (apvts)
{
}

void BitTrixerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setPatch (paramCache.read());
    engine.prepare (sampleRate, samplesPerBlock);
}

bool BitTrixerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void BitTrixerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    engine.setPatch (paramCache.read());

    // ---- UI trigger requests (ons first, so a click within one block
    //      starts the sound; min-gate then defers the off) ----
    for (int ons = uiGateOnRequests.exchange (0); ons > 0; --ons)
        engine.manualGateOn();
    for (int offs = uiGateOffRequests.exchange (0); offs > 0; --offs)
        engine.manualGateOff();
    for (int shots = uiOneShotRequests.exchange (0); shots > 0; --shots)
        engine.oneShot();

    // ---- MIDI gating (block-quantized; plenty for SFX work) ----
    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        const bool chOk = midiLearn.acceptsChannel (msg.getChannel());
        if (msg.isNoteOn())
        {
            if (chOk) engine.noteOn (msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            if (chOk) engine.noteOff (msg.getNoteNumber());
        }
        else if (msg.isController())
        {
            if (chOk) midiLearn.handleCc (msg.getControllerNumber(), msg.getControllerValue());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int n = 0; n < 128; ++n)
                engine.noteOff (n);
            engine.manualGateOff();
        }
    }
    midi.clear();

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() >= 2)
    {
        engine.render (buffer.getWritePointer (0),
                       buffer.getWritePointer (1), numSamples);
    }
    else
    {
        buffer.clear();
    }
}

juce::AudioProcessorEditor* BitTrixerProcessor::createEditor()
{
    return new BitTrixerEditor (*this);
}

void BitTrixerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // wrap the parameter tree and the MIDI map under one root
    juce::ValueTree root ("BITTRIXER");
    root.appendChild (apvts.copyState(), nullptr);
    midiLearn.saveTo (root);
    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void BitTrixerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (tree.hasType ("BITTRIXER"))
    {
        auto params = tree.getChildWithName (apvts.state.getType());
        if (params.isValid())
            apvts.replaceState (params);
        midiLearn.loadFrom (tree);
    }
    else
    {
        apvts.replaceState (tree);   // legacy: bare parameter tree
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BitTrixerProcessor();
}
