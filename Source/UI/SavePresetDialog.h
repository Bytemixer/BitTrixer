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

#include "OverlayPanel.h"
#include "../PresetManager.h"

// ============================================================================
//  SavePresetDialog — the SAVE overlay: name, user folder (the folder named
//  after the Generate category that made the sound is offered by default),
//  author (remembered), description and tags. Asks before overwriting.
// ============================================================================

class SavePresetDialog : public OverlayPanel
{
public:
    SavePresetDialog (PresetManager& manager, juce::PropertiesFile& settingsFile)
        : OverlayPanel ("SAVE PRESET", 560, 430), presets (manager), props (settingsFile)
    {
        auto label = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            addAndMakeVisible (l);
        };
        label (nameLabel, "Name");
        label (folderLabel, "Folder");
        label (authorLabel, "Author");
        label (descLabel, "Description");
        label (tagsLabel, "Tags");

        descEditor.setMultiLine (true, true);
        descEditor.setReturnKeyStartsNewLine (true);
        tagsEditor.setTextToShowWhenEmpty ("comma, separated", RetroColors::textDim);
        nameEditor.onReturnKey = [this] { requestSave(); };
        nameEditor.onEscapeKey = [this] { setVisible (false); };
        for (auto* e : { &nameEditor, &authorEditor, &descEditor, &tagsEditor })
            addAndMakeVisible (e);
        addAndMakeVisible (folderBox);

        saveButton.setButtonText ("SAVE");
        saveButton.setColour (juce::TextButton::buttonColourId, RetroColors::accentDark);
        saveButton.onClick = [this] { requestSave(); };
        cancelButton.setButtonText ("CANCEL");
        cancelButton.onClick = [this] { setVisible (false); };
        addAndMakeVisible (saveButton);
        addAndMakeVisible (cancelButton);
    }

    void open()
    {
        auto& lib = presets.library();
        const auto* cur = lib.find (presets.getCurrentId());
        nameEditor.setText (presets.getCurrentName(), false);
        authorEditor.setText (props.getValue (kAuthorKey), false);
        descEditor.setText (cur != nullptr ? cur->meta.description : juce::String(), false);
        tagsEditor.setText (cur != nullptr ? cur->meta.tags.joinIntoString (", ") : juce::String(), false);

        // folder choices: Unsorted, every user folder, and the category's folder
        // the category's folder takes the spelling of a matching user or factory folder
        const auto& cat = presets.getSourceCategory();
        const auto userFolders = lib.userFolders();
        const auto factoryFolders = lib.factoryFolders();
        auto categoryFolder = cat.substring (0, 1).toUpperCase() + cat.substring (1).toLowerCase();
        for (const auto* names : { &userFolders, &factoryFolders })
            if (const int i = names->indexOf (categoryFolder, true); i >= 0)
            {
                categoryFolder = (*names)[i];
                break;
            }
        folderChoices.clear();
        folderChoices.add ({});
        folderChoices.addArray (userFolders);
        if (categoryFolder.isNotEmpty())
            folderChoices.addIfNotAlreadyThere (categoryFolder);
        folderBox.clear (juce::dontSendNotification);
        for (int i = 0; i < folderChoices.size(); ++i)
            folderBox.addItem (folderChoices[i].isEmpty() ? juce::String ("Unsorted")
                                                          : folderChoices[i], i + 1);

        juce::String preferred = cur != nullptr && ! cur->factory ? cur->folder : categoryFolder;
        folderBox.setSelectedId (juce::jmax (0, folderChoices.indexOf (preferred)) + 1, juce::dontSendNotification);
        show();
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }

    void resized() override
    {
        auto b = contentBounds();
        constexpr int labelW = 100;
        auto row = [&b] (int h) { auto r = b.removeFromTop (h); b.removeFromTop (8); return r; };
        auto r = row (28);
        nameLabel.setBounds (r.removeFromLeft (labelW));
        nameEditor.setBounds (r);
        r = row (28);
        folderLabel.setBounds (r.removeFromLeft (labelW));
        folderBox.setBounds (r);
        r = row (28);
        authorLabel.setBounds (r.removeFromLeft (labelW));
        authorEditor.setBounds (r);
        r = row (28);
        tagsLabel.setBounds (r.removeFromLeft (labelW));
        tagsEditor.setBounds (r);

        auto buttons = b.removeFromBottom (30);
        cancelButton.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (8);
        saveButton.setBounds (buttons.removeFromRight (110));
        b.removeFromBottom (10);
        r = b;
        descLabel.setBounds (r.removeFromLeft (labelW).removeFromTop (28));
        descEditor.setBounds (r);
    }

private:
    void requestSave()
    {
        const auto name = nameEditor.getText().trim();
        if (juce::File::createLegalFileName (name).isEmpty())
            return;
        const auto folder = folderChoices[juce::jmax (0, folderBox.getSelectedId() - 1)];
        if (presets.library().userFile (name, folder).existsAsFile())
        {
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon, "Overwrite preset?",
                "\"" + name + "\" already exists in " + (folder.isEmpty() ? juce::String ("Unsorted") : folder) + ".",
                "Overwrite", "Cancel", this,
                juce::ModalCallbackFunction::create (
                    [safe = juce::Component::SafePointer<SavePresetDialog> (this), name, folder] (int result)
                    {
                        if (safe != nullptr && result == 1) safe->doSave (name, folder);
                    }));
            return;
        }
        doSave (name, folder);
    }

    void doSave (const juce::String& name, const juce::String& folder)
    {
        PresetLibrary::Meta m;
        m.author = authorEditor.getText().trim();
        m.description = descEditor.getText().trim();
        m.tags.addTokens (tagsEditor.getText(), ",", "");
        m.tags.trim();
        m.tags.removeEmptyStrings();
        props.setValue (kAuthorKey, m.author);
        props.saveIfNeeded();
        if (presets.save (name, folder, m).isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                    "Could not write the preset file.", "OK", this);
            return;
        }
        setVisible (false);
    }

    static constexpr const char* kAuthorKey = "presetAuthor";

    PresetManager& presets;
    juce::PropertiesFile& props;
    juce::StringArray folderChoices;

    juce::Label nameLabel, folderLabel, authorLabel, descLabel, tagsLabel;
    juce::TextEditor nameEditor, authorEditor, descEditor, tagsEditor;
    juce::ComboBox folderBox;
    juce::TextButton saveButton, cancelButton;
};
