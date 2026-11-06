/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Benjamin Arnaud <bunjee@omega.gg>
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

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Menus

Widgets.PageExt {
    id: root

    signal showList(var model, int reason)

    property alias currentIndex: videoAll.currentIndex

    // NOTE: We are exposing a custom SortMenu with grouping options.
    sortMenu: SortMenuVideo {
        ctx: MainCtx

        onGrouping: (grouping) => { MainCtx.grouping = grouping }
    }

    readonly property QtObject _meta: {
        const grouping = MainCtx.grouping;

        if (grouping === MainCtx.GROUPING_NAME)
            return groupComponent.createObject()
        else if (grouping === MainCtx.GROUPING_FOLDER)
            return folderComponent.createObject()
        else
            return videoComponent.createObject()
    }

    on_MetaChanged: {
        // Purge the objects that are pending deletion, to have the model
        // preservation behavior aligned with 460cd3d4. If we don't do
        // this, the QML/JS engine would pick an arbitrary time to collect
        // garbage, potentially leading having all models alive at the
        // same time:
        // > having the three models always present, means that the data (at
        // > least the first chuck) is loaded 3 times, and will be reloaded
        // > 3 times every time a database event triggers a refresh of the
        // > model.
        gc() // `QJSEngine::GarbageCollectionExtension` is installed by default
    }

    // Functions

    title: qsTr("Videos")

    VideoAll {
        id: videoAll

        anchors.fill: parent

        focus: true

        model: _meta?.model ?? null

        contextMenu: MLContextMenu { model: _meta ? _meta.model : null; showPlayAsAudioAction: true }

        gridLabels: _meta?.gridLabels ?? videoAll.getLabel

        showGroupCountColumn: _meta?.showGroupCountColumn ?? false

        sortModel: _meta?.sortModel ?? [
            { text: qsTr("Alphabetic"), criteria: "title"    },
            { text: qsTr("Duration"),   criteria: "duration" }
        ]

        sectionProperty: _meta?.sectionProperty ?? ""

        displayMarginBeginning: root.displayMarginBeginning
        displayMarginEnd: root.displayMarginEnd
        enableBeginningFade: root.enableBeginningFade
        enableEndFade: root.enableEndFade


        // VideoAll events reimplementation

        function getLabelGroup(model, string) {
            if (!model) return ""

            if (model.isVideo)
                return getLabel(model)
            return [ string.arg(model.count) ]
        }


        function onAction(indexes) { _meta.onAction(indexes) }

        function onDoubleClick(object) { _meta.onDoubleClick(object) }

        function isInfoExpandPanelAvailable(modelIndexData) {
            return _meta.isInfoExpandPanelAvailable(modelIndexData)
        }

        // Children

        Component {
            id: videoComponent

            QtObject {
                id: metaVideo

                property var model: MLVideoModel {
                    ml: MediaLib
                    searchPattern: root.search.pattern
                    sortOrder: root.sort.order
                    sortCriteria: root.sort.criteria
                }

                property var gridLabels: videoAll.getLabel

                property string sectionProperty: {
                    switch (model.sortCriteria) {
                    case "title":
                        return "title_first_symbol"
                    default:
                        return ""
                    }
                }

                function onAction(indexes) {
                    model.addAndPlay( indexes )
                    MainCtx.playerView = true
                }

                function onDoubleClick(object) {
                    MediaLib.addAndPlay(object.id)
                    MainCtx.playerView = true
                }

                function isInfoExpandPanelAvailable(modelIndexData) { return true }
            }
        }

        Component {
            id: groupComponent

            QtObject {
                id: metaGroup

                property var model: MLVideoGroupsModel {
                    ml: MediaLib
                    searchPattern: root.search.pattern
                    sortOrder: root.sort.order
                    sortCriteria: root.sort.criteria
                }

                property string sectionProperty: {
                    switch (model.sortCriteria) {
                    case "title":
                        return "group_title_first_symbol"
                    default:
                        return ""
                    }
                }

                property var gridLabels: function (model) {
                    return videoAll.getLabelGroup(model, qsTr("%1 Videos"))
                }

                property bool showGroupCountColumn: true

                function onAction(indexes) {
                    const index = indexes[0]

                    const object = model.getDataAt(index);

                    if (object.isVideo) {
                        model.addAndPlay( indexes )
                        MainCtx.playerView = true

                        return
                    }

                    root.showList(object, Qt.TabFocusReason)
                }

                function onDoubleClick(object) {
                    if (object.isVideo) {
                        MediaLib.addAndPlay(object.id)
                        MainCtx.playerView = true
                        return
                    }

                    root.showList(object, Qt.MouseFocusReason)
                }

                function isInfoExpandPanelAvailable(modelIndexData) {
                    return modelIndexData.isVideo
                }
            }
        }

        Component {
            id: folderComponent

            QtObject {
                id: metaFolder

                property var model: MLVideoFoldersModel {
                    ml: MediaLib
                    searchPattern: root.search.pattern
                    sortOrder: root.sort.order
                    sortCriteria: root.sort.criteria
                }

                property string sectionProperty: {
                    switch (model.sortCriteria) {
                    case "title":
                        return "title_first_symbol"
                    default:
                        return ""
                    }
                }

                property var gridLabels: function (model) {
                    return videoAll.getLabelGroup(model, qsTr("%1 Videos"))
                }

                property bool showGroupCountColumn: true

                // Terrikon: folders and videos, as on the disk
                property var sortModel: [
                    { text: qsTr("Alphabetic"), criteria: "title" },
                    { text: qsTr("Number of videos"), criteria: "nb_videos" },
                    { text: qsTr("Length"), criteria: "duration" },
                    { text: qsTr("Size"), criteria: "size_text" },
                    { text: qsTr("Date modified"), criteria: "mtime_text" },
                    { text: qsTr("Last watched"), criteria: "last_played_text" }
                ]

                function onAction(indexes) {
                    const index = indexes[0]
                    const object = model.getDataAt(index)

                    if (object.isVideo) {
                        model.addAndPlay(indexes)
                        MainCtx.playerView = true
                        return
                    }

                    root.showList(object, Qt.TabFocusReason)
                }

                function onDoubleClick(object) {
                    if (object.isVideo) {
                        MediaLib.addAndPlay(object.id)
                        MainCtx.playerView = true
                        return
                    }

                    root.showList(object, Qt.MouseFocusReason)
                }

                function isInfoExpandPanelAvailable(modelIndexData) {
                    return !!modelIndexData.isVideo
                }
            }
        }
    }
}
