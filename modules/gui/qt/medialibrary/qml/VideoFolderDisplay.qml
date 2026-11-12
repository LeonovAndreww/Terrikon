/*****************************************************************************
 * Copyright (C) 2026 Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
import QtQuick

import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Menus

import VLC.Widgets as Widgets
import VLC.Style

// A folder of Videos opened: its subfolders (which open the same way, or
// expand in place) and its own videos
Widgets.PageExt {
    id: root

    property alias currentIndex: videoAll.currentIndex
    property alias parentId: folderModel.parentId

    signal showList(var model, int reason)

    title: qsTr("Videos")

    sortMenu: SortMenuVideo {
        ctx: MainCtx

        onGrouping: (grouping) => { MainCtx.grouping = grouping }
    }

    // Terrikon: the page's content, so that its header gets the sort, the
    // search and the grid / list buttons
    contentItem: VideoAll {
        id: videoAll

        focus: true

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd
        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade

        sectionProperty: (model.sortCriteria === "title") ? "title_first_symbol" : ""

        sortModel: [
            { text: qsTr("Alphabetic"), criteria: "title" },
            { text: qsTr("Number of videos"), criteria: "nb_videos" },
            { text: qsTr("Length"), criteria: "duration" },
            { text: qsTr("Size"), criteria: "size_text" },
            { text: qsTr("Date modified"), criteria: "mtime_text" },
            { text: qsTr("Last watched"), criteria: "last_played_text" }
        ]

        showGroupCountColumn: true

        gridLabels: (item) => {
            if (!item)
                return ""
            if (item.isVideo)
                return videoAll.getLabel(item)
            return [ qsTr("%1 Videos").arg(item.count) ]
        }

        function isInfoExpandPanelAvailable(modelIndexData) {
            return !!modelIndexData.isVideo
        }

        function onAction(indexes) {
            const object = folderModel.getDataAt(indexes[0])
            if (object.isVideo) {
                folderModel.addAndPlay(indexes)
                MainCtx.playerView = true
                return
            }
            root.showList(object, Qt.TabFocusReason)
        }

        function onDoubleClick(object) {
            if (object.isVideo) {
                // Terrikon: the rest of its folder comes after it
                MediaLib.playWithFolder(object.id)
                MainCtx.playerView = true
                return
            }
            root.showList(object, Qt.MouseFocusReason)
        }

        model: MLVideoFoldersModel {
            id: folderModel

            ml: MediaLib
            searchPattern: root.search.pattern
            sortOrder: root.sort.order
            sortCriteria: root.sort.criteria
        }

        contextMenu: MLContextMenu { model: folderModel; showPlayAsAudioAction: true }
    }
}
