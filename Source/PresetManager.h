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
#include "PresetLibrary.h"

// ============================================================================
//  PresetManager — the current preset: loads library presets or any .rfxp
//  file into the APVTS, steps through the current preset's folder, saves the
//  current sound into the library, and snapshots the sound so a browse can be
//  undone. Message-thread only.
// ============================================================================

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& state, PresetLibrary& presetLibrary)
        : apvts (state), lib (presetLibrary) {}

    PresetLibrary& library() noexcept { return lib; }

    bool load (const PresetLibrary::Preset& p)
    {
        auto xml = lib.readXml (p);
        if (xml == nullptr || ! apply (*xml))
            return false;
        currentName = p.name;
        currentId = p.id;
        sourceCategory = p.meta.category;
        lib.touchRecent (p.id);
        if (onPresetChanged) onPresetChanged();
        return true;
    }

    // LOAD: any .rfxp file, inside the library or not
    void loadAsync()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load preset", lib.userRoot(),
                                                       juce::String ("*") + PresetLibrary::kExtension);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                auto xml = juce::XmlDocument::parse (file);
                if (xml == nullptr || ! apply (*xml))
                    return;
                currentName = file.getFileNameWithoutExtension();
                currentId = file.isAChildOf (lib.userRoot())
                              ? PresetLibrary::userId (file, lib.userRoot()) : juce::String();
                sourceCategory = PresetLibrary::readMeta (*xml).category;
                if (onPresetChanged) onPresetChanged();
            });
    }

    // the previous/next preset in the current preset's folder, wrapping
    void step (int dir)
    {
        const auto* cur = lib.find (currentId);
        std::vector<const PresetLibrary::Preset*> list;
        for (const auto& p : lib.all())
            if (cur == nullptr || (p.factory == cur->factory && p.folder == cur->folder))
                list.push_back (&p);
        if (list.empty())
            return;
        const int n = (int) list.size();
        int idx = -1;
        for (int i = 0; i < n; ++i)
            if (list[(size_t) i]->id == currentId) { idx = i; break; }
        load (*list[(size_t) (idx < 0 ? (dir > 0 ? 0 : n - 1) : (idx + dir + n) % n)]);
    }

    // saves the current sound into the library; returns the new id or ""
    juce::String save (const juce::String& name, const juce::String& folder, PresetLibrary::Meta meta)
    {
        meta.category = sourceCategory;
        const auto id = lib.save (apvts.copyState(), name, folder, meta);
        if (id.isNotEmpty())
        {
            currentId = id;
            currentName = lib.find (id) != nullptr ? lib.find (id)->name : name;
            lib.touchRecent (id);
            if (onPresetChanged) onPresetChanged();
        }
        return id;
    }

    // ---- a sound made by RANDOM or a Generate category ----
    void setGenerated (const juce::String& name, const juce::String& category)
    {
        currentName = name;
        currentId = {};
        sourceCategory = category;
    }

    const juce::String& getCurrentName() const noexcept { return currentName; }
    const juce::String& getCurrentId() const noexcept   { return currentId; }
    const juce::String& getSourceCategory() const noexcept { return sourceCategory; }

    // ---- undo a browse ----
    struct Snapshot { juce::ValueTree state; juce::String name, id, category; };

    Snapshot snapshot() const { return { apvts.copyState(), currentName, currentId, sourceCategory }; }

    void restore (const Snapshot& s)
    {
        apvts.replaceState (s.state.createCopy());
        currentName = s.name;
        currentId = s.id;
        sourceCategory = s.category;
        if (onPresetChanged) onPresetChanged();
    }

    std::function<void()> onPresetChanged;

private:
    bool apply (const juce::XmlElement& xml)
    {
        auto tree = PresetLibrary::stateWithoutMeta (xml);
        if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
            return false;
        apvts.replaceState (tree);
        return true;
    }

    juce::AudioProcessorValueTreeState& apvts;
    PresetLibrary& lib;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String currentName { "Init" }, currentId, sourceCategory;
};
