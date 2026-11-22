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
//  AppSettings — the per-user settings file (theme, export locations, preset
//  favourites and recent), "BitTrixer.prefs" next to the standalone wrapper's
//  own "BitTrixer.settings". Hold it through juce::SharedResourcePointer so
//  every plugin instance in a process shares one in-memory copy.
// ============================================================================

class AppSettings
{
public:
    AppSettings()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "BitTrixer";
        o.folderName = "BitTrixer";
        o.filenameSuffix = "prefs";
        o.osxLibrarySubFolder = "Application Support";
        props = std::make_unique<juce::PropertiesFile> (o);
        migrateFrom (o.getDefaultFile().withFileExtension ("settings"));
    }

    juce::PropertiesFile& file() noexcept { return *props; }

private:
    // the first run copies the theme from the file shared with the standalone wrapper
    void migrateFrom (const juce::File& old)
    {
        if (props->containsKey ("themeIndex") || ! old.existsAsFile())
            return;
        juce::PropertiesFile previous (old, {});
        for (const auto& key : previous.getAllProperties().getAllKeys())
            if (key == "themeIndex" || key.startsWith ("custom_"))
                props->setValue (key, previous.getValue (key));
        props->saveIfNeeded();
    }

    std::unique_ptr<juce::PropertiesFile> props;
};
