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

#include <vector>
#include <algorithm>
#include <juce_audio_processors/juce_audio_processors.h>

// ============================================================================
//  PresetLibrary — the index of every preset: USER presets (.rfxp files in
//  Documents/BitTrixer Presets, one level of folders) and FACTORY presets
//  (read-only, compiled in). Metadata lives in a META child of the preset XML;
//  favourites and recent are kept in the shared settings file by preset id
//  ("user:<relative path>" or "factory:<path>"). Message-thread only.
// ============================================================================

class PresetLibrary
{
public:
    static constexpr const char* kExtension = ".rfxp";
    static constexpr const char* kMetaTag = "META";

    struct Meta
    {
        juce::String author, description, category;
        juce::StringArray tags;
    };

    struct Preset
    {
        juce::String id, name, folder;      // folder "" = the user root
        bool factory = false;
        juce::File file;                    // user presets
        int factoryIndex = -1;              // factory presets
        Meta meta;
    };

    // one compiled-in preset: path "Folder/Name.rfxp" and its XML text
    struct FactoryItem { juce::String path; juce::String xml; };

    PresetLibrary (juce::PropertiesFile& settingsFile, std::vector<FactoryItem> factoryItems = {},
                   juce::File userFolder = defaultUserRoot())
        : props (settingsFile), factory (std::move (factoryItems)), root (std::move (userFolder))
    {
        root.createDirectory();
        rescan();
    }

    static juce::File defaultUserRoot()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("BitTrixer Presets");
    }

    const juce::File& userRoot() const noexcept { return root; }

    // ---- index ----
    void rescan()
    {
        presets.clear();
        auto addUser = [this] (const juce::File& f, const juce::String& folder)
        {
            Preset p;
            p.file = f;
            p.folder = folder;
            p.name = f.getFileNameWithoutExtension();
            p.id = userId (f, root);
            if (auto xml = juce::XmlDocument::parse (f))
                p.meta = readMeta (*xml);
            presets.push_back (std::move (p));
        };
        for (auto& f : root.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension))
            addUser (f, {});
        for (auto& dir : root.findChildFiles (juce::File::findDirectories, false))
            for (auto& f : dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExtension))
                addUser (f, dir.getFileName());

        for (int i = 0; i < (int) factory.size(); ++i)
        {
            const auto& item = factory[(size_t) i];
            Preset p;
            p.factory = true;
            p.factoryIndex = i;
            p.folder = item.path.upToLastOccurrenceOf ("/", false, false);
            p.name = item.path.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
            p.id = "factory:" + item.path;
            if (auto xml = juce::XmlDocument::parse (item.xml))
                p.meta = readMeta (*xml);
            presets.push_back (std::move (p));
        }

        std::sort (presets.begin(), presets.end(), [] (const Preset& a, const Preset& b)
        {
            if (a.factory != b.factory) return b.factory;     // user first
            if (a.folder != b.folder)   return a.folder.compareIgnoreCase (b.folder) < 0;
            return a.name.compareIgnoreCase (b.name) < 0;
        });
    }

    const std::vector<Preset>& all() const noexcept { return presets; }

    const Preset* find (const juce::String& id) const
    {
        for (const auto& p : presets)
            if (p.id == id) return &p;
        return nullptr;
    }

    // user folders on disk (including empty ones), sorted
    juce::StringArray userFolders() const
    {
        juce::StringArray names;
        for (auto& dir : userRoot().findChildFiles (juce::File::findDirectories, false))
            names.add (dir.getFileName());
        names.sortNatural();
        return names;
    }

    juce::StringArray factoryFolders() const
    {
        juce::StringArray names;
        for (const auto& p : presets)
            if (p.factory) names.addIfNotAlreadyThere (p.folder);
        names.sortNatural();
        return names;
    }

    // every word of the query must appear in the name, folder, description or tags
    static bool matches (const Preset& p, const juce::String& query)
    {
        const auto hay = (p.name + " " + p.folder + " " + p.meta.description + " "
                          + p.meta.tags.joinIntoString (" ") + " " + p.meta.category).toLowerCase();
        for (const auto& word : juce::StringArray::fromTokens (query.toLowerCase(), true))
            if (word.isNotEmpty() && ! hay.contains (word))
                return false;
        return true;
    }

    // ---- reading ----
    std::unique_ptr<juce::XmlElement> readXml (const Preset& p) const
    {
        if (p.factory)
            return juce::isPositiveAndBelow (p.factoryIndex, (int) factory.size())
                     ? juce::XmlDocument::parse (factory[(size_t) p.factoryIndex].xml) : nullptr;
        return juce::XmlDocument::parse (p.file);
    }

    static Meta readMeta (const juce::XmlElement& xml)
    {
        Meta m;
        if (auto* e = xml.getChildByName (kMetaTag))
        {
            m.author      = e->getStringAttribute ("author");
            m.description = e->getStringAttribute ("description");
            m.category    = e->getStringAttribute ("category");
            m.tags.addTokens (e->getStringAttribute ("tags"), ",", "");
            m.tags.trim();
            m.tags.removeEmptyStrings();
        }
        return m;
    }

    // the parameter state alone: the META child never reaches the APVTS
    static juce::ValueTree stateWithoutMeta (const juce::XmlElement& xml)
    {
        auto tree = juce::ValueTree::fromXml (xml);
        auto meta = tree.getChildWithName (kMetaTag);
        if (meta.isValid())
            tree.removeChild (meta, nullptr);
        return tree;
    }

    // ---- favourites / recent ----
    bool isFavourite (const juce::String& id) const { return idList (kFavKey).contains (id); }

    void setFavourite (const juce::String& id, bool on)
    {
        auto list = idList (kFavKey);
        list.removeString (id);
        if (on) list.add (id);
        storeIdList (kFavKey, list);
    }

    juce::StringArray recent() const { return idList (kRecentKey); }

    void touchRecent (const juce::String& id)
    {
        auto list = idList (kRecentKey);
        list.removeString (id);
        list.insert (0, id);
        list.removeRange (kMaxRecent, list.size());
        storeIdList (kRecentKey, list);
    }

    // ---- user edits; each returns the resulting preset id, or "" on failure ----
    juce::String save (const juce::ValueTree& state, const juce::String& name,
                       const juce::String& folder, const Meta& meta)
    {
        const auto file = userFile (name, folder);
        if (file == juce::File() || ! file.getParentDirectory().createDirectory().wasOk())
            return {};
        auto xml = state.createXml();
        if (xml == nullptr)
            return {};
        if (auto* old = xml->getChildByName (kMetaTag))
            xml->removeChildElement (old, true);
        xml->addChildElement (metaXml (meta).release());
        if (! xml->writeTo (file))
            return {};
        rescan();
        return userId (file, userRoot());
    }

    // rewrite a user preset's metadata, renaming the file if the name changed
    juce::String updateMeta (const Preset& p, const juce::String& newName, const Meta& meta)
    {
        if (p.factory)
            return {};
        auto xml = juce::XmlDocument::parse (p.file);
        if (xml == nullptr)
            return {};
        if (auto* old = xml->getChildByName (kMetaTag))
            xml->removeChildElement (old, true);
        xml->addChildElement (metaXml (meta).release());

        auto target = p.file;
        const auto legal = juce::File::createLegalFileName (newName.trim());
        if (legal.isNotEmpty() && legal != p.name)
        {
            target = p.file.getSiblingFile (legal + kExtension);
            if (target.exists())
                return {};
        }
        if (! xml->writeTo (target))
            return {};
        if (target != p.file)
            p.file.deleteFile();
        return reindex (p.id, userId (target, userRoot()));
    }

    juce::String move (const Preset& p, const juce::String& folder)
    {
        if (p.factory || p.folder == folder)
            return {};
        const auto target = userFile (p.name, folder);
        if (target == juce::File() || target.exists()
            || ! target.getParentDirectory().createDirectory().wasOk() || ! p.file.moveFileTo (target))
            return {};
        return reindex (p.id, userId (target, userRoot()));
    }

    bool remove (const Preset& p)
    {
        if (p.factory || ! p.file.moveToTrash())
            return false;
        reindex (p.id, {});
        return true;
    }

    bool createFolder (const juce::String& name)
    {
        const auto legal = juce::File::createLegalFileName (name.trim());
        return legal.isNotEmpty() && userRoot().getChildFile (legal).createDirectory().wasOk();
    }

    bool renameFolder (const juce::String& from, const juce::String& to)
    {
        const auto legal = juce::File::createLegalFileName (to.trim());
        const auto src = userRoot().getChildFile (from), dst = userRoot().getChildFile (legal);
        if (from.isEmpty() || legal.isEmpty() || dst.exists() || ! src.moveFileTo (dst))
            return false;
        const auto oldPrefix = "user:" + from + "/", newPrefix = "user:" + legal + "/";
        for (const char* key : { kFavKey, kRecentKey })
        {
            auto list = idList (key);
            for (auto& id : list)
                if (id.startsWith (oldPrefix)) id = newPrefix + id.substring (oldPrefix.length());
            storeIdList (key, list);
        }
        rescan();
        return true;
    }

    // moves the folder and everything in it to the recycle bin
    bool removeFolder (const juce::String& name)
    {
        if (name.isEmpty() || ! userRoot().getChildFile (name).moveToTrash())
            return false;
        rescan();
        return true;
    }

    juce::File userFile (const juce::String& name, const juce::String& folder) const
    {
        const auto legal = juce::File::createLegalFileName (name.trim());
        if (legal.isEmpty())
            return {};
        const auto dir = folder.isEmpty() ? userRoot()
                                          : userRoot().getChildFile (juce::File::createLegalFileName (folder));
        return dir.getChildFile (legal + kExtension);
    }

    static juce::String userId (const juce::File& f, const juce::File& root)
    {
        return "user:" + f.getRelativePathFrom (root).replaceCharacter ('\\', '/');
    }

private:
    static std::unique_ptr<juce::XmlElement> metaXml (const Meta& m)
    {
        auto e = std::make_unique<juce::XmlElement> (kMetaTag);
        e->setAttribute ("author", m.author);
        e->setAttribute ("description", m.description);
        e->setAttribute ("category", m.category);
        e->setAttribute ("tags", m.tags.joinIntoString (", "));
        return e;
    }

    // after a file moved or vanished: carry favourites/recent over, then rescan
    juce::String reindex (juce::String oldId, juce::String newId)
    {
        for (const char* key : { kFavKey, kRecentKey })
        {
            auto list = idList (key);
            const int i = list.indexOf (oldId);
            if (i >= 0)
            {
                if (newId.isEmpty()) list.remove (i);
                else                 list.set (i, newId);
                storeIdList (key, list);
            }
        }
        rescan();
        return newId;
    }

    juce::StringArray idList (const char* key) const
    {
        auto list = juce::StringArray::fromLines (props.getValue (key));
        list.removeEmptyStrings();
        return list;
    }

    void storeIdList (const char* key, const juce::StringArray& list)
    {
        props.setValue (key, list.joinIntoString ("\n"));
        props.saveIfNeeded();
    }

    static constexpr const char* kFavKey    = "presetFavourites";
    static constexpr const char* kRecentKey = "presetRecent";
    static constexpr int kMaxRecent = 20;

    juce::PropertiesFile& props;
    std::vector<FactoryItem> factory;
    juce::File root;
    std::vector<Preset> presets;
};
