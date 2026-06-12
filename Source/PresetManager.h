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

#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  PresetManager — saves/loads the whole APVTS state as .rfxp XML files in
//  Documents/RetroForge Presets. Message-thread only (async FileChoosers).
// ============================================================================

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

    static juce::File defaultDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("RetroForge Presets");
        dir.createDirectory();
        return dir;
    }

    void saveAsync()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Save preset", defaultDirectory().getChildFile (currentName + ".rfxp"), "*.rfxp");

        chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File())
                    return;
                if (auto xml = apvts.copyState().createXml())
                {
                    xml->writeTo (file.withFileExtension ("rfxp"));
                    currentName = file.getFileNameWithoutExtension();
                    if (onPresetChanged) onPresetChanged();
                }
            });
    }

    void loadAsync()
    {
        chooser = std::make_unique<juce::FileChooser> (
            "Load preset", defaultDirectory(), "*.rfxp");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File() || ! file.existsAsFile())
                    return;
                if (auto xml = juce::XmlDocument::parse (file))
                {
                    auto tree = juce::ValueTree::fromXml (*xml);
                    if (tree.isValid() && tree.hasType (apvts.state.getType()))
                    {
                        apvts.replaceState (tree);
                        currentName = file.getFileNameWithoutExtension();
                        if (onPresetChanged) onPresetChanged();
                    }
                }
            });
    }

    const juce::String& getCurrentName() const noexcept { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }

    std::function<void()> onPresetChanged;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String currentName { "Init" };
};
