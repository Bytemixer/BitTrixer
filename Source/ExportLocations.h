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

#include <juce_data_structures/juce_data_structures.h>

// ============================================================================
//  ExportLocations — the user's export folders and which one is the default,
//  kept in the shared BitTrixer settings file (plugin and standalone alike).
// ============================================================================

class ExportLocations
{
public:
    explicit ExportLocations (juce::PropertiesFile& settingsFile) : props (settingsFile) {}

    juce::Array<juce::File> list() const
    {
        juce::Array<juce::File> dirs;
        for (const auto& path : juce::StringArray::fromLines (props.getValue (kListKey)))
            if (path.isNotEmpty())
                dirs.add (juce::File (path));
        return dirs;
    }

    // -1 when the list is empty
    int defaultIndex() const
    {
        const int n = list().size();
        return n == 0 ? -1 : juce::jlimit (0, n - 1, props.getIntValue (kDefaultKey, 0));
    }

    // the default location, or Documents when none is set
    juce::File defaultLocation() const
    {
        const auto dirs = list();
        const int i = defaultIndex();
        return i >= 0 ? dirs[i]
                      : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    void add (const juce::File& dir)
    {
        auto dirs = list();
        if (! dirs.contains (dir))
        {
            dirs.add (dir);
            store (dirs, defaultIndex() < 0 ? 0 : defaultIndex());
        }
    }

    void remove (int index)
    {
        auto dirs = list();
        if (! juce::isPositiveAndBelow (index, dirs.size()))
            return;
        int def = defaultIndex();
        dirs.remove (index);
        if (index < def || def >= dirs.size())
            --def;
        store (dirs, juce::jmax (0, def));
    }

    void setDefault (int index)
    {
        if (juce::isPositiveAndBelow (index, list().size()))
        {
            props.setValue (kDefaultKey, index);
            props.saveIfNeeded();
        }
    }

private:
    void store (const juce::Array<juce::File>& dirs, int def)
    {
        juce::StringArray paths;
        for (const auto& f : dirs)
            paths.add (f.getFullPathName());
        props.setValue (kListKey, paths.joinIntoString ("\n"));
        props.setValue (kDefaultKey, def);
        props.saveIfNeeded();
    }

    static constexpr const char* kListKey    = "exportLocations";
    static constexpr const char* kDefaultKey = "exportDefaultLocation";
    juce::PropertiesFile& props;
};
