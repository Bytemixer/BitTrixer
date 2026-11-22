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

#include <FactoryData.h>
#include "PresetLibrary.h"

// ============================================================================
//  FactoryBank — the compiled-in factory presets (Presets/Factory in the
//  repo). FactoryIndex.txt lists each preset's "Folder/Name.rfxp"; the
//  matching resource is found by its original file name.
// ============================================================================

namespace FactoryBank
{
    inline std::vector<PresetLibrary::FactoryItem> load()
    {
        std::vector<PresetLibrary::FactoryItem> items;
        int indexSize = 0;
        const char* index = FactoryData::getNamedResource ("FactoryIndex_txt", indexSize);
        if (index == nullptr)
            return items;

        for (const auto& path : juce::StringArray::fromLines (juce::String::fromUTF8 (index, indexSize)))
        {
            const auto fileName = path.fromLastOccurrenceOf ("/", false, false);
            if (fileName.isEmpty())
                continue;
            for (int i = 0; i < FactoryData::namedResourceListSize; ++i)
            {
                if (fileName != FactoryData::originalFilenames[i])
                    continue;
                int size = 0;
                if (const char* data = FactoryData::getNamedResource (FactoryData::namedResourceList[i], size))
                    items.push_back ({ path, juce::String::fromUTF8 (data, size) });
                break;
            }
        }
        return items;
    }
}
