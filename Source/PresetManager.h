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

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <algorithm>

// ============================================================================
//  PresetManager — saves/loads the whole APVTS state as .rfxp XML files in
//  Documents/BitTrixer Presets. Scans that folder (one level of subfolders =
//  categories) so the UI can browse/step presets. Message-thread only.
// ============================================================================

class PresetManager
{
public:
    struct Entry { juce::File file; juce::String name, category; };

    explicit PresetManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        rescan();
    }

    static juce::File defaultDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                       .getChildFile ("BitTrixer Presets");
        dir.createDirectory();
        return dir;
    }

    // ---- folder scan: top-level files are uncategorised; one level of
    //      subfolders (Sweeps, Gusts, Explosions, ...) become categories ----
    void rescan()
    {
        entries.clear();
        auto root = defaultDirectory();
        for (auto& f : root.findChildFiles (juce::File::findFiles, false, "*.rfxp"))
            entries.push_back ({ f, f.getFileNameWithoutExtension(), {} });
        for (auto& sub : root.findChildFiles (juce::File::findDirectories, false))
            for (auto& f : sub.findChildFiles (juce::File::findFiles, false, "*.rfxp"))
                entries.push_back ({ f, f.getFileNameWithoutExtension(), sub.getFileName() });

        std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b)
        {
            if (a.category != b.category) return a.category < b.category;   // "" (uncategorised) first
            return a.name.compareIgnoreCase (b.name) < 0;
        });
    }

    const std::vector<Entry>& getPresets() const noexcept { return entries; }

    bool loadFile (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr)
            return false;
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
            return false;
        apvts.replaceState (tree);
        currentName = file.getFileNameWithoutExtension();
        currentFile = file;
        if (onPresetChanged) onPresetChanged();
        return true;
    }

    // step to the prev/next preset in the (category, name)-sorted list, wrapping
    void step (int dir)
    {
        if (entries.empty()) { rescan(); if (entries.empty()) return; }
        int idx = -1;
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].file == currentFile) { idx = (int) i; break; }
        const int n = (int) entries.size();
        if (idx < 0)
            loadFile (entries[(size_t) (dir > 0 ? 0 : n - 1)].file);
        else
            loadFile (entries[(size_t) ((idx + dir + n) % n)].file);
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
                    file = file.withFileExtension ("rfxp");
                    xml->writeTo (file);
                    currentName = file.getFileNameWithoutExtension();
                    currentFile = file;
                    rescan();                       // so the new save shows up in the browser
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
                if (file != juce::File())
                    loadFile (file);
            });
    }

    const juce::String& getCurrentName() const noexcept { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; currentFile = juce::File(); }

    std::function<void()> onPresetChanged;

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String currentName { "Init" };
    juce::File currentFile;
    std::vector<Entry> entries;
};
