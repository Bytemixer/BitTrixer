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
//  PresetBrowser — the preset library overlay. Left: search and the folder
//  list (All, Favourites, Recent, USER folders, FACTORY folders). Middle: the
//  presets in that view; a click loads and plays one, a double-click or Enter
//  keeps it and closes, Esc or CANCEL restores the sound from before. Right:
//  the selected preset's info (name, author, description, tags, favourite).
//  Drag a user preset onto a user folder to move it.
// ============================================================================

class PresetBrowser : public OverlayPanel,
                      public juce::DragAndDropContainer,
                      private juce::ListBoxModel,
                      private juce::KeyListener
{
public:
    explicit PresetBrowser (PresetManager& manager)
        : OverlayPanel ("PRESETS", 0, 0), presets (manager), lib (manager.library())
    {
        search.setTextToShowWhenEmpty ("Search name, description, tags", RetroColors::textDim);
        search.onTextChange = [this] { refreshList(); };
        search.onReturnKey  = [this] { keepAndClose(); };
        search.onEscapeKey  = [this] { cancel(); };
        search.addKeyListener (this);
        addAndMakeVisible (search);

        folders.onSelect = [this] { refreshList(); updateFolderButtons(); };
        folders.onDropPreset = [this] (const juce::String& id, const juce::String& folder) { movePreset (id, folder); };
        folderView.setViewedComponent (&folders, false);
        folderView.setScrollBarsShown (true, false);
        addAndMakeVisible (folderView);

        list.setModel (this);
        list.setRowHeight (26);
        list.setColour (juce::ListBox::backgroundColourId, RetroColors::track);
        list.setColour (juce::ListBox::outlineColourId, RetroColors::panelEdge);
        list.setOutlineThickness (1);
        list.addKeyListener (this);
        addAndMakeVisible (list);

        auto label = [this] (juce::Label& l, const char* text)
        {
            l.setText (text, juce::dontSendNotification);
            addAndMakeVisible (l);
        };
        label (nameLabel, "Name");
        label (folderLabel, "Folder");
        label (authorLabel, "Author");
        label (categoryLabel, "Made by");
        label (descLabel, "Description");
        label (tagsLabel, "Tags");
        folderValue.setColour (juce::Label::textColourId, RetroColors::textDim);
        categoryValue.setColour (juce::Label::textColourId, RetroColors::textDim);
        addAndMakeVisible (folderValue);
        addAndMakeVisible (categoryValue);

        descEditor.setMultiLine (true, true);
        descEditor.setReturnKeyStartsNewLine (true);
        tagsEditor.setTextToShowWhenEmpty ("comma, separated", RetroColors::textDim);
        for (auto* e : { &nameEditor, &authorEditor, &descEditor, &tagsEditor })
            addAndMakeVisible (e);

        favToggle.setButtonText ("Favourite");
        favToggle.onClick = [this] { toggleFavourite(); };
        addAndMakeVisible (favToggle);

        auto button = [this] (juce::TextButton& b, const char* text, std::function<void()> fn)
        {
            b.setButtonText (text);
            b.onClick = std::move (fn);
            addAndMakeVisible (b);
        };
        button (saveInfoButton, "SAVE INFO", [this] { saveInfo(); });
        button (deletePresetButton, "DELETE", [this] { deletePreset(); });
        button (newFolderButton, "+ FOLDER", [this] { newFolder(); });
        button (renameFolderButton, "RENAME", [this] { renameFolder(); });
        button (deleteFolderButton, "DELETE", [this] { deleteFolder(); });
        button (keepButton, "KEEP AND CLOSE", [this] { keepAndClose(); });
        button (cancelButton, "CANCEL", [this] { cancel(); });
        keepButton.setColour (juce::TextButton::buttonColourId, RetroColors::accentDark);
        cancelButton.setTooltip ("Close and restore the sound from before browsing (Esc)");
    }

    ~PresetBrowser() override
    {
        search.removeKeyListener (this);
        list.removeKeyListener (this);
    }

    // shows the library and remembers the current sound for CANCEL
    void open()
    {
        before = presets.snapshot();
        lib.rescan();
        rebuildFolders();
        refreshList();
        selectId (presets.getCurrentId());
        show();
        search.grabKeyboardFocus();
    }

    std::function<void()> onAudition;     // play the loaded preset

    void resized() override
    {
        auto b = contentBounds();
        auto bottom = b.removeFromBottom (30);
        cancelButton.setBounds (bottom.removeFromRight (120));
        bottom.removeFromRight (8);
        keepButton.setBounds (bottom.removeFromRight (160));
        b.removeFromBottom (10);

        auto left = b.removeFromLeft (juce::jmin (260, b.getWidth() / 4));
        b.removeFromLeft (10);
        auto right = b.removeFromRight (juce::jmin (320, b.getWidth() / 3));
        b.removeFromRight (10);

        search.setBounds (left.removeFromTop (28));
        left.removeFromTop (8);
        auto folderButtons = left.removeFromBottom (28);
        const int fw = (folderButtons.getWidth() - 12) / 3;
        newFolderButton.setBounds (folderButtons.removeFromLeft (fw));
        folderButtons.removeFromLeft (6);
        renameFolderButton.setBounds (folderButtons.removeFromLeft (fw));
        folderButtons.removeFromLeft (6);
        deleteFolderButton.setBounds (folderButtons);
        left.removeFromBottom (6);
        folderView.setBounds (left);
        layoutFolders();

        list.setBounds (b);

        auto row = [&right] (int h) { auto r = right.removeFromTop (h); right.removeFromTop (6); return r; };
        nameLabel.setBounds (row (22));
        nameEditor.setBounds (row (28));
        auto r = row (22);
        folderLabel.setBounds (r.removeFromLeft (70));
        folderValue.setBounds (r);
        r = row (22);
        categoryLabel.setBounds (r.removeFromLeft (70));
        categoryValue.setBounds (r);
        authorLabel.setBounds (row (22));
        authorEditor.setBounds (row (28));
        tagsLabel.setBounds (row (22));
        tagsEditor.setBounds (row (28));
        favToggle.setBounds (row (26));
        auto buttons = right.removeFromBottom (28);
        deletePresetButton.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        saveInfoButton.setBounds (buttons.removeFromRight (120));
        right.removeFromBottom (6);
        descLabel.setBounds (row (22));
        descEditor.setBounds (right);
    }

private:
    // ---------------------------------------------------------------- folders
    // the left column: views and folders; user folders accept dropped presets.
    // Sits in a Viewport, which it scrolls while a drag rests near its edge.
    struct FolderList : public juce::Component, public juce::DragAndDropTarget, private juce::Timer
    {
        enum class Kind { All, Favourites, Recent, Heading, User, Factory };
        struct Node { Kind kind; juce::String label, folder; };

        void rebuild (const PresetLibrary& lib)
        {
            const auto keep = selected >= 0 && selected < (int) nodes.size() ? nodes[(size_t) selected] : Node {};
            nodes.clear();
            nodes.push_back ({ Kind::All, "All presets", {} });
            nodes.push_back ({ Kind::Favourites, "Favourites", {} });
            nodes.push_back ({ Kind::Recent, "Recent", {} });
            nodes.push_back ({ Kind::Heading, "USER", {} });
            nodes.push_back ({ Kind::User, "Unsorted", {} });
            for (const auto& f : lib.userFolders())
                nodes.push_back ({ Kind::User, f, f });
            const auto factory = lib.factoryFolders();
            if (! factory.isEmpty())
            {
                nodes.push_back ({ Kind::Heading, "FACTORY", {} });
                for (const auto& f : factory)
                    nodes.push_back ({ Kind::Factory, f, f });
            }
            selected = 0;
            for (int i = 0; i < (int) nodes.size(); ++i)
                if (nodes[(size_t) i].kind == keep.kind && nodes[(size_t) i].folder == keep.folder
                    && nodes[(size_t) i].kind != Kind::Heading)
                    selected = i;
            repaint();
        }

        const Node& current() const { return nodes[(size_t) juce::jlimit (0, (int) nodes.size() - 1, selected)]; }

        bool currentIsUserFolder() const { return current().kind == Kind::User && current().folder.isNotEmpty(); }

        int contentHeight() const { return 2 + (int) nodes.size() * kRowH; }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (RetroColors::track);
            for (int i = 0; i < (int) nodes.size(); ++i)
            {
                const auto& n = nodes[(size_t) i];
                auto r = rowBounds (i);
                if (i == selected)
                {
                    g.setColour (RetroColors::accentDark.withAlpha (0.6f));
                    g.fillRect (r);
                }
                if (i == dropHover)
                {
                    g.setColour (RetroColors::accent);
                    g.drawRect (r, 2);
                }
                const bool heading = n.kind == Kind::Heading;
                const bool nested  = n.kind == Kind::User || n.kind == Kind::Factory;
                g.setColour (heading ? RetroColors::panelTitle : RetroColors::text);
                g.setFont (juce::Font (juce::FontOptions (heading ? 12.5f : 14.5f,
                                                          heading ? juce::Font::bold : juce::Font::plain)));
                g.drawText (n.label, r.withTrimmedLeft (nested ? 22 : 10), juce::Justification::centredLeft, true);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = rowAt (e.y);
            if (i >= 0 && nodes[(size_t) i].kind != Kind::Heading)
            {
                selected = i;
                repaint();
                if (onSelect) onSelect();
            }
        }

        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            return d.description.toString().startsWith ("user:");
        }

        void itemDragEnter (const SourceDetails&) override { startTimerHz (30); }

        void itemDragMove (const SourceDetails& d) override { setHover (d.localPosition.y); }

        void itemDragExit (const SourceDetails&) override
        {
            stopTimer();
            dropHover = -1;
            repaint();
        }

        void itemDropped (const SourceDetails& d) override
        {
            stopTimer();
            const int i = rowAt (d.localPosition.y);
            dropHover = -1;
            repaint();
            if (i >= 0 && nodes[(size_t) i].kind == Kind::User && onDropPreset)
                onDropPreset (d.description.toString(), nodes[(size_t) i].folder);
        }

        std::function<void()> onSelect;
        std::function<void (const juce::String& id, const juce::String& folder)> onDropPreset;

    private:
        void setHover (int y)
        {
            const int i = rowAt (y);
            const int hover = i >= 0 && nodes[(size_t) i].kind == Kind::User ? i : -1;
            if (hover != dropHover) { dropHover = hover; repaint(); }
        }

        // scrolls the viewport while the dragged preset rests near its top or bottom edge
        void timerCallback() override
        {
            auto* view = findParentComponentOfClass<juce::Viewport>();
            if (view == nullptr)
                return;
            const auto mouse = juce::Desktop::getMousePosition();
            const auto inView = view->getLocalPoint (nullptr, mouse);
            if (view->autoScroll (inView.x, inView.y, 28, 10))
                setHover (getLocalPoint (nullptr, mouse).y);
        }

        static constexpr int kRowH = 26;
        juce::Rectangle<int> rowBounds (int i) const { return { 1, 1 + i * kRowH, getWidth() - 2, kRowH }; }
        int rowAt (int y) const
        {
            const int i = (y - 1) / kRowH;
            return juce::isPositiveAndBelow (i, (int) nodes.size()) ? i : -1;
        }

        std::vector<Node> nodes;
        int selected = 0, dropHover = -1;
    };

    // ---------------------------------------------------------------- list
    void refreshList()
    {
        using K = FolderList::Kind;
        const auto& node = folders.current();
        const auto query = search.getText();
        const auto recent = lib.recent();
        shown.clear();
        if (node.kind == K::Recent)
        {
            for (const auto& id : recent)
                if (const auto* p = lib.find (id); p != nullptr && PresetLibrary::matches (*p, query))
                    shown.push_back (p->id);
        }
        else
        {
            for (const auto& p : lib.all())
            {
                const bool inView = node.kind == K::All
                                 || (node.kind == K::Favourites && lib.isFavourite (p.id))
                                 || (node.kind == K::User && ! p.factory && p.folder == node.folder)
                                 || (node.kind == K::Factory && p.factory && p.folder == node.folder);
                if (inView && PresetLibrary::matches (p, query))
                    shown.push_back (p.id);
            }
        }
        list.updateContent();
        list.repaint();
        selectId (selectedId);
    }

    // selects without loading: selectedId is set first, so selectedRowsChanged sees no change
    void selectId (const juce::String& id)
    {
        selectedId = id;
        const auto it = std::find (shown.begin(), shown.end(), id);
        if (it != shown.end())
            list.selectRow ((int) (it - shown.begin()), false, true);
        else
            list.deselectAllRows();
        showInfo();
    }

    const PresetLibrary::Preset* selectedPreset() const { return lib.find (selectedId); }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size()))
            return;
        const auto* p = lib.find (shown[(size_t) row]);
        if (p == nullptr)
            return;
        if (isSelected)
            g.fillAll (RetroColors::accentDark.withAlpha (0.6f));
        auto r = juce::Rectangle<int> (0, 0, width, height).reduced (8, 0);
        g.setFont (juce::Font (juce::FontOptions (14.5f)));
        g.setColour (RetroColors::accent);
        g.drawText (lib.isFavourite (p->id) ? "*" : "", r.removeFromLeft (14), juce::Justification::centredLeft);
        g.setColour (RetroColors::textDim);
        const auto where = (p->factory ? "Factory / " : "") + (p->folder.isEmpty() ? juce::String ("Unsorted") : p->folder);
        g.drawText (where, r.removeFromRight (juce::jmin (220, width / 3)), juce::Justification::centredLeft, true);
        g.setColour (RetroColors::text);
        g.drawText (p->name, r, juce::Justification::centredLeft, true);
    }

    // loads and plays the preset on a row. A new click reports both a selection
    // change and a click; the second report of one row within 200 ms is dropped.
    void auditionRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size()))
            return;
        selectedId = shown[(size_t) row];
        showInfo();
        const auto now = juce::Time::getMillisecondCounter();
        if (selectedId == lastAuditionId && now - lastAuditionMs < 200)
            return;
        lastAuditionId = selectedId;
        lastAuditionMs = now;
        if (const auto* p = selectedPreset(); p != nullptr && presets.load (*p) && onAudition)
            onAudition();
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override { auditionRow (row); }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size()))
            return;
        if (shown[(size_t) row] != presets.getCurrentId())
            auditionRow (row);
        keepAndClose();
    }

    void returnKeyPressed (int) override { keepAndClose(); }

    // keyboard selection plays the new row; a click already did
    void selectedRowsChanged (int row) override
    {
        if (juce::isPositiveAndBelow (row, (int) shown.size()) && shown[(size_t) row] != selectedId)
            auditionRow (row);
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() == 1 && juce::isPositiveAndBelow (rows[0], (int) shown.size()))
            return shown[(size_t) rows[0]];
        return {};
    }

    // up/down from the search box move through the list
    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::escapeKey) { cancel(); return true; }
        if (key == juce::KeyPress::returnKey) { keepAndClose(); return true; }
        const int dir = key == juce::KeyPress::downKey ? 1 : key == juce::KeyPress::upKey ? -1 : 0;
        if (dir == 0 || shown.empty())
            return false;
        const int row = juce::jlimit (0, (int) shown.size() - 1, list.getSelectedRow() + dir);
        list.selectRow (row);
        return true;
    }

    // ---------------------------------------------------------------- info
    void showInfo()
    {
        const auto* p = selectedPreset();
        const bool editable = p != nullptr && ! p->factory;
        nameEditor.setText (p != nullptr ? p->name : juce::String(), false);
        folderValue.setText (p == nullptr ? juce::String()
                             : (p->factory ? "Factory / " : "") + (p->folder.isEmpty() ? juce::String ("Unsorted") : p->folder),
                             juce::dontSendNotification);
        categoryValue.setText (p != nullptr ? p->meta.category : juce::String(), juce::dontSendNotification);
        authorEditor.setText (p != nullptr ? p->meta.author : juce::String(), false);
        descEditor.setText (p != nullptr ? p->meta.description : juce::String(), false);
        tagsEditor.setText (p != nullptr ? p->meta.tags.joinIntoString (", ") : juce::String(), false);
        favToggle.setToggleState (p != nullptr && lib.isFavourite (p->id), juce::dontSendNotification);
        for (juce::Component* c : { (juce::Component*) &nameEditor, (juce::Component*) &authorEditor,
                                    (juce::Component*) &descEditor, (juce::Component*) &tagsEditor,
                                    (juce::Component*) &saveInfoButton, (juce::Component*) &deletePresetButton })
            c->setEnabled (editable);
        for (auto* e : { &nameEditor, &authorEditor, &descEditor, &tagsEditor })
            e->setReadOnly (! editable);
        favToggle.setEnabled (p != nullptr);
    }

    void saveInfo()
    {
        const auto* p = selectedPreset();
        if (p == nullptr || p->factory)
            return;
        PresetLibrary::Meta m = p->meta;
        m.author = authorEditor.getText().trim();
        m.description = descEditor.getText().trim();
        m.tags.clear();
        m.tags.addTokens (tagsEditor.getText(), ",", "");
        m.tags.trim();
        m.tags.removeEmptyStrings();
        const bool wasCurrent = p->id == presets.getCurrentId();
        const auto newId = lib.updateMeta (*p, nameEditor.getText(), m);
        if (newId.isEmpty())
        {
            showMessage ("Could not save the info. Another preset in this folder may already use that name.");
            return;
        }
        if (wasCurrent)
            if (const auto* np = lib.find (newId)) presets.load (*np);
        selectedId = newId;
        refreshList();
    }

    void toggleFavourite()
    {
        if (const auto* p = selectedPreset())
            lib.setFavourite (p->id, favToggle.getToggleState());
        refreshList();
    }

    void deletePreset()
    {
        const auto* p = selectedPreset();
        if (p == nullptr || p->factory)
            return;
        confirm ("Delete preset?", "Move \"" + p->name + "\" to the recycle bin?", "Delete",
                 [this, id = p->id]
                 {
                     if (const auto* q = lib.find (id); q != nullptr && lib.remove (*q))
                     {
                         selectedId = {};
                         refreshList();
                     }
                 });
    }

    void movePreset (const juce::String& id, const juce::String& folder)
    {
        const auto* p = lib.find (id);
        if (p == nullptr || p->factory || p->folder == folder)
            return;
        const auto newId = lib.move (*p, folder);
        if (newId.isEmpty())
        {
            showMessage ("Could not move the preset. That folder may already have a preset with this name.");
            return;
        }
        if (presets.getCurrentId() == id)
            if (const auto* np = lib.find (newId)) presets.load (*np);
        selectedId = newId;
        refreshList();
    }

    // ---------------------------------------------------------------- folder ops
    void rebuildFolders()
    {
        folders.rebuild (lib);
        layoutFolders();
        updateFolderButtons();
    }

    // the folder list fills the view width, less the scrollbar when it overflows
    void layoutFolders()
    {
        const int h = folders.contentHeight();
        const bool overflows = h > folderView.getHeight();
        folders.setSize (folderView.getWidth() - (overflows ? folderView.getScrollBarThickness() : 0),
                         juce::jmax (h, folderView.getHeight()));
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        g.setColour (RetroColors::panelEdge);
        g.drawRect (folderView.getBounds());
    }

    void updateFolderButtons()
    {
        renameFolderButton.setEnabled (folders.currentIsUserFolder());
        deleteFolderButton.setEnabled (folders.currentIsUserFolder());
    }

    void newFolder()
    {
        askText ("New folder", "Folder name", {}, [this] (const juce::String& name)
        {
            if (! lib.createFolder (name))
                return showMessage ("Could not create that folder.");
            rebuildFolders();
            refreshList();
        });
    }

    void renameFolder()
    {
        if (! folders.currentIsUserFolder())
            return;
        const auto from = folders.current().folder;
        askText ("Rename folder", "New name", from, [this, from] (const juce::String& to)
        {
            if (to.trim() == from)
                return;
            if (! lib.renameFolder (from, to))
                return showMessage ("Could not rename the folder. That name may already be in use.");
            rebuildFolders();
            refreshList();
        });
    }

    void deleteFolder()
    {
        if (! folders.currentIsUserFolder())
            return;
        const auto name = folders.current().folder;
        confirm ("Delete folder?", "Move the folder \"" + name + "\" and every preset in it to the recycle bin?",
                 "Delete", [this, name]
                 {
                     if (lib.removeFolder (name))
                     {
                         rebuildFolders();
                         refreshList();
                     }
                 });
    }

    // ---------------------------------------------------------------- closing
    void keepAndClose() { setVisible (false); }

    void cancel()
    {
        presets.restore (before);
        setVisible (false);
    }

    bool canDismiss() const override { return false; }

    // ---------------------------------------------------------------- dialogs
    void showMessage (const juce::String& text)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Presets", text, "OK", this);
    }

    void confirm (const juce::String& heading, const juce::String& text, const juce::String& okText,
                  std::function<void()> onOk)
    {
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, heading, text, okText, "Cancel", this,
            juce::ModalCallbackFunction::create (
                [safe = juce::Component::SafePointer<PresetBrowser> (this), onOk] (int result)
                {
                    if (safe != nullptr && result == 1) onOk();
                }));
    }

    void askText (const juce::String& heading, const juce::String& label, const juce::String& initial,
                  std::function<void (const juce::String&)> onOk)
    {
        auto* w = new juce::AlertWindow (heading, {}, juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor ("text", initial, label);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create (
            [safe = juce::Component::SafePointer<PresetBrowser> (this), w, onOk] (int result)
            {
                if (safe != nullptr && result == 1)
                    onOk (w->getTextEditorContents ("text"));
            }), true);
    }

    PresetManager& presets;
    PresetLibrary& lib;
    PresetManager::Snapshot before;
    std::vector<juce::String> shown;
    juce::String selectedId, lastAuditionId;
    juce::uint32 lastAuditionMs = 0;

    juce::TextEditor search;
    FolderList folders;
    juce::Viewport folderView;
    juce::ListBox list;
    juce::Label nameLabel, folderLabel, authorLabel, categoryLabel, descLabel, tagsLabel;
    juce::Label folderValue, categoryValue;
    juce::TextEditor nameEditor, authorEditor, descEditor, tagsEditor;
    juce::ToggleButton favToggle;
    juce::TextButton saveInfoButton, deletePresetButton, newFolderButton, renameFolderButton, deleteFolderButton;
    juce::TextButton keepButton, cancelButton;
};
