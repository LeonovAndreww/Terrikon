/******************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Author: Ash <ashutoshv191@gmail.com>
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
 ******************************************************************************/
import QtQuick
import QtQuick.Window
import QtQuick.Templates as T

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style
import VLC.Terrikon

Widgets.PageExt {
    id: root

    property var pagePrefix: [] // behave like a Page

    property var sortModel: [
        { text: qsTr("Alphabetic"), criteria: "title" },
        { text: qsTr("Duration"), criteria: "duration" }
    ]

    title: qsTr("Home")

    hasGridListMode: true

    isSearchable: true

    property real listCoverHeight: VLCStyle.listAlbumCover_height
    property real listCoverWidth: VLCStyle.listAlbumCover_width
    property real listCoverRadius: VLCStyle.listAlbumCover_radius

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    signal seeAllButtonClicked(string name, int reason)

    function positionContentAtBeginning() {
        contentYBehavior.enabled = true
        flickable.contentY = -flickable.originY
        contentYBehavior.enabled = false
    }

    function setCurrentItemFocus(reason) {
        // `focus` is checked, because that indicates the item that has the focus within the focus scope
        if (continueWatchingRow.focus)
            continueWatchingRow.setCurrentItemFocus(reason)
        else if (favoritesRow.focus)
            favoritesRow.setCurrentItemFocus(reason)
        else if (newVideoRow.focus)
            newVideoRow.setCurrentItemFocus(reason)
        else if (newMusicRow.focus)
            newMusicRow.setCurrentItemFocus(reason)
        else
            coneNButtons.forceActiveFocus(reason)
    }

    contentItem: Flickable {
        id: flickable

        implicitWidth: Math.max(coneNButtons.implicitWidth, mediaRows.implicitWidth)
        implicitHeight: coneNButtons.implicitHeight + foldersCard.height
                        + (_hasMedias ? mediaRows.implicitHeight : 0)

        contentWidth: width
        contentHeight: _hasMedias ? coneNButtons.implicitHeight + foldersCard.height + mediaRows.implicitHeight
                                  : height - topMargin - bottomMargin

        flickableDirection: Flickable.AutoFlickIfNeeded

        boundsBehavior: Flickable.StopAtBounds

        // FIXME: make the media rows positioners consider the flickable margins when adjusting flickable `contentY`.
        bottomMargin: VLCStyle.margin_large

        pixelAligned: (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density

        property bool _hasMedias: true

        Binding on _hasMedias {
            when: !continueWatchingRow.model.loading && !favoritesRow.model.loading
                  && !newVideoRow.model.loading && !newMusicRow.model.loading
            value: continueWatchingRow.visible || favoritesRow.visible
                   || newVideoRow.visible || newMusicRow.visible
            restoreMode: Binding.RestoreNone
        }

        T.ScrollBar.vertical: Widgets.ScrollBarExt {}

        // This behavior allows to have similar "smooth" animation
        // that Qt views have with `highlightFollowsCurrentItem`.
        Behavior on contentY {
            id: contentYBehavior

            enabled: false

            // NOTE: Usage of `SmoothedAnimation` is intentional here.
            SmoothedAnimation {
                duration: VLCStyle.duration_veryLong
                easing.type: Easing.InOutSine
            }
        }

        property Component implicitFlickableScrollHandler: DefaultFlickableScrollHandler { }

        // NOTE: This property can be set to null to prevent using a scroll handler:
        property FlickableScrollHandler scrollHandler: {
            if (interactive) {
                // JS ownership:
                return implicitFlickableScrollHandler.createObject(null, { target: flickable })
            } else {
                return null
            }
        }

        Component.onCompleted: {
            if (!usingTouch) {
                // Flickable filters child mouse events for flicking (even when
                // the delegate is grabbed). However, this is not a useful
                // feature for non-touch cases, so disable it here and enable
                // it if touch is detected through the hover handler:
                MainCtx.setFiltersChildMouseEvents(flickable, false)
            }
        }

        readonly property bool usingTouch: MainCtx.usingTouch

        onUsingTouchChanged: {
            if (usingTouch)
                MainCtx.setFiltersChildMouseEvents(flickable, true)
            // We do not disable filtering child mouse events
            // because Qt currently has a bug that the flickable
            // jumps when it is enabled again later on.
        }

        NoMedialibHome.ConeNButtons {
            id: coneNButtons

            focus: true

            orientation: flickable._hasMedias ? Qt.Horizontal : Qt.Vertical

            anchors.centerIn: flickable._hasMedias ? undefined : parent
            anchors.top: flickable._hasMedias ? parent.top : undefined
            anchors.left: flickable._hasMedias ? parent.left : undefined
            anchors.leftMargin: flickable._hasMedias ? newVideoRow.contentLeftMargin : 0

            Navigation.parentItem: root
            Navigation.upItem: root.header
            Navigation.downItem: mediaRows

            onActiveFocusChanged: {
                contentYBehavior.enabled = true
                Helpers.positionFlickableToContainItem(flickable, this)
                contentYBehavior.enabled = false
            }
        }

        // Terrikon: the folders of the media library, from here rather
        // than from the preferences. With none yet, the folders where game
        // recorders and Windows keep videos are offered, one click each.
        MLFolderModel {
            id: mlFolders
            ctx: MainCtx

            onModelReset: {
                // the folders come asynchronously: until then, "none" is not known
                foldersCard.loaded = true
                foldersCard.refresh()
            }
            onRowsInserted: foldersCard.refresh()
            onRowsRemoved: foldersCard.refresh()
        }

        Item {
            id: foldersCard

            property int folderCount: 0
            property bool loaded: false
            property var suggestions: []

            function refresh() {
                folderCount = mlFolders.rowCount()
                const have = []
                for (let i = 0; i < folderCount; ++i)
                    have.push(String(mlFolders.data(mlFolders.index(i, 0), 259 /* MRL */)))
                MediaFolders.requestSuggestions(have)
            }

            Connections {
                target: MediaFolders
                function onSuggestionsReady(found) { foldersCard.suggestions = found }
            }

            function addFolder(url) {
                if (url.toString() !== "")
                    mlFolders.add(url)
            }

            Component.onCompleted: refresh()

            readonly property ColorContext colorContext: ColorContext {
                id: cardTheme
                colorSet: ColorContext.View
            }

            anchors.top: coneNButtons.bottom
            anchors.topMargin: VLCStyle.margin_normal
            anchors.left: flickable._hasMedias ? parent.left : undefined
            anchors.leftMargin: flickable._hasMedias ? newVideoRow.contentLeftMargin : 0
            anchors.horizontalCenter: flickable._hasMedias ? undefined : parent.horizontalCenter

            width: Math.min(VLCStyle.dp(620, VLCStyle.scale), flickable.width - 2 * VLCStyle.margin_large)
            height: card.visible ? card.implicitHeight + VLCStyle.margin_normal
                  : (addLine.visible ? addLine.implicitHeight + VLCStyle.margin_small : 0)

            // No folder yet: what to add
            Rectangle {
                id: card

                visible: foldersCard.loaded && foldersCard.folderCount === 0
                width: parent.width
                implicitHeight: cardColumn.implicitHeight + 2 * VLCStyle.margin_normal
                radius: VLCStyle.dp(10, VLCStyle.scale)
                color: Qt.rgba(1, 1, 1, 0.05)
                border.color: Qt.rgba(1, 1, 1, 0.08)

                Column {
                    id: cardColumn

                    anchors.fill: parent
                    anchors.margins: VLCStyle.margin_normal
                    spacing: VLCStyle.margin_small

                    Widgets.SubtitleLabel {
                        text: qsTr("Add your video folders")
                        color: cardTheme.fg.primary
                    }

                    Widgets.CaptionLabel {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Their videos show up here and in Videos, grouped by folder, and new recordings appear on their own.")
                        color: cardTheme.fg.secondary
                    }

                    Repeater {
                        model: foldersCard.suggestions

                        Item {
                            required property var modelData

                            width: cardColumn.width
                            height: VLCStyle.dp(48, VLCStyle.scale)

                            Column {
                                anchors.left: parent.left
                                anchors.right: addButton.left
                                anchors.rightMargin: VLCStyle.margin_small
                                anchors.verticalCenter: parent.verticalCenter

                                Widgets.ListLabel {
                                    width: parent.width
                                    text: parent.parent.modelData.title + "  ·  "
                                          + qsTr("%1 videos").arg(parent.parent.modelData.videos)
                                    color: cardTheme.fg.primary
                                }

                                Widgets.CaptionLabel {
                                    width: parent.width
                                    elide: Text.ElideMiddle
                                    text: parent.parent.modelData.path
                                    color: cardTheme.fg.secondary
                                }
                            }

                            Widgets.ButtonExt {
                                id: addButton
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("Add")
                                onClicked: foldersCard.addFolder(parent.modelData.url)
                            }
                        }
                    }

                    Widgets.ButtonExt {
                        text: qsTr("Choose another folder…")
                        onClicked: foldersCard.addFolder(MediaFolders.pickFolder())
                    }
                }
            }

            // Folders there already: each one, removable, and one more
            Flow {
                id: addLine

                visible: !card.visible && flickable._hasMedias
                width: parent.width
                spacing: VLCStyle.margin_xsmall

                Repeater {
                    model: mlFolders

                    // A folder: its name, the whole path on hover; the cross
                    // takes it out of the media library (the files stay)
                    Rectangle {
                        id: chip

                        required property int index
                        required property var display_url

                        readonly property string _path: decodeURIComponent(String(display_url)).replace(/^file:\/\/\/?/, "")
                        readonly property string _name: _path.replace(/[\\\/]+$/, "").split(/[\\\/]/).pop()

                        height: VLCStyle.dp(30, VLCStyle.scale)
                        width: chipRow.implicitWidth + 2 * VLCStyle.margin_small
                        radius: height / 2
                        color: chipHover.hovered ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                        border.color: Qt.rgba(1, 1, 1, 0.1)

                        HoverHandler { id: chipHover }

                        Widgets.ToolTipExt {

                            colorContext.palette: VLCStyle.palette
                            visible: chipHover.hovered && !removeButton.hovered
                            text: chip._path
                        }

                        Row {
                            id: chipRow

                            anchors.centerIn: parent
                            spacing: VLCStyle.margin_xsmall

                            Widgets.IconLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                text: VLCIcons.ic_fluent_folder_open_24_regular
                                color: cardTheme.fg.secondary
                                font.pixelSize: VLCStyle.icon_small
                            }

                            Widgets.CaptionLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                text: chip._name
                                color: cardTheme.fg.primary
                            }

                            Widgets.IconToolButton {
                                id: removeButton
                                anchors.verticalCenter: parent.verticalCenter
                                width: VLCStyle.dp(22, VLCStyle.scale)
                                height: width
                                font.pixelSize: VLCStyle.icon_xsmall
                                text: VLCIcons.close
                                description: qsTr("Remove from the media library (the files stay)")
                                onClicked: mlFolders.removeAt(chip.index)
                            }
                        }
                    }
                }

                // Offered and not there: a dashed chip, added on click
                Repeater {
                    model: foldersCard.suggestions

                    Rectangle {
                        id: offer

                        required property var modelData

                        height: VLCStyle.dp(30, VLCStyle.scale)
                        width: offerRow.implicitWidth + 2 * VLCStyle.margin_small
                        radius: height / 2
                        color: offerHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"
                        border.color: Qt.rgba(1, 1, 1, 0.18)

                        HoverHandler { id: offerHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: foldersCard.addFolder(offer.modelData.url) }

                        Widgets.ToolTipExt {

                            colorContext.palette: VLCStyle.palette
                            visible: offerHover.hovered
                            text: qsTr("Add %1 to the media library").arg(offer.modelData.path)
                        }

                        Row {
                            id: offerRow

                            anchors.centerIn: parent
                            spacing: VLCStyle.margin_xsmall

                            Widgets.IconLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                text: VLCIcons.add
                                color: cardTheme.fg.secondary
                                font.pixelSize: VLCStyle.icon_xsmall
                            }

                            Widgets.CaptionLabel {
                                anchors.verticalCenter: parent.verticalCenter
                                text: offer.modelData.title
                                color: cardTheme.fg.secondary
                            }
                        }
                    }
                }

                Widgets.ButtonExt {
                    height: VLCStyle.dp(30, VLCStyle.scale)
                    iconTxt: VLCIcons.add
                    text: qsTr("Add folder")
                    onClicked: foldersCard.addFolder(MediaFolders.pickFolder())
                }
            }
        }

        Widgets.NavigableCol {
            id: mediaRows

            anchors.top: foldersCard.bottom
            anchors.left: parent.left
            anchors.right: parent.right

            spacing: 0 // NOTE: We depend on the top padding of view header currently.

            Navigation.parentItem: root
            Navigation.upItem: coneNButtons

            Widgets.ViewHeader {
                visible: continueWatchingRow.visible
                text: qsTr("Continue Watching")
                view: continueWatchingRow
                seeAllButton.visible: continueWatchingRow.model.maximumCount > continueWatchingRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("continueWatching", reason)
                }
            }

            VideoAll {
                id: continueWatchingRow

                AccessibleCompat.id: "continueWatching"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                sectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLRecentVideoModel {
                    ml: MediaLib

                    sortCriteria: root.sort.criteria
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    chunkSize: VLCStyle.gridView_maxNbItemPerRow_wide

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(continueWatchingRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                contextMenu: MLContextMenu {
                    model: continueWatchingRow.model

                    showPlayAsAudioAction: true
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = false
                        }
                    }
                }
            }

            Widgets.ViewHeader {
                visible: favoritesRow.visible
                text: qsTr("Favorites")
                view: favoritesRow
                seeAllButton.visible: favoritesRow.model.maximumCount > favoritesRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("favorites", reason)
                }
            }

            MediaView {
                id: favoritesRow

                AccessibleCompat.id: "favourites"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLMediaModel {
                    favoriteOnly: true

                    ml: MediaLib

                    sortCriteria: root.sort.criteria || "insertion"
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    chunkSize: VLCStyle.gridView_maxNbItemPerRow_square

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(favoritesRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = true
                        }
                    }
                }
            }

            Widgets.ViewHeader {
                text: qsTr("New Videos")
                visible: newVideoRow.visible
                view: newVideoRow
                seeAllButton.visible: newVideoRow.model.maximumCount > newVideoRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("newVideo", reason)
                }
            }

            VideoAll {
                id: newVideoRow

                AccessibleCompat.id: "newVideo"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                headerPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                sectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLVideoModel {
                    ml: MediaLib

                    sortCriteria: root.sort.criteria || "insertion"
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    chunkSize: VLCStyle.gridView_maxNbItemPerRow_wide

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(newVideoRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                contextMenu: MLContextMenu {
                    model: newVideoRow.model

                    showPlayAsAudioAction: true
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = false
                        }
                    }
                }
            }

            Widgets.ViewHeader {
                text: qsTr("New Music")
                visible: newMusicRow.visible
                view: newMusicRow
                seeAllButton.visible: newMusicRow.model.maximumCount > newMusicRow.model.count

                onSeeAllButtonClicked: function (reason) {
                    root.seeAllButtonClicked("newMusic", reason)
                }
            }

            MediaView {
                id: newMusicRow

                AccessibleCompat.id: "newMusic"

                anchors.left: parent.left
                anchors.right: parent.right

                height: currentItem?.contentHeight ?? implicitHeight

                visible: model.count !== 0

                listHeaderPositioning: ListView.InlineHeader
                enableBeginningFade: false
                enableEndFade: false
                listSectionProperty: ""

                interactive: false

                emptyLabel: null

                // FIXME: `ExpandGridView` causes extreme performance degradation when `reuseItems`
                //        is true and items provided by the model change (#29084).
                reuseItems: !MainCtx.gridView

                listCoverWidth: root.listCoverWidth
                listCoverHeight: root.listCoverHeight
                listCoverRadius: root.listCoverRadius

                Navigation.parentItem: mediaRows

                model: MLAudioModel {
                    ml: MediaLib

                    sortCriteria: root.sort.criteria || "insertion"
                    sortOrder: root.sort.order
                    searchPattern: root.search.pattern

                    chunkSize: VLCStyle.gridView_maxNbItemPerRow_square

                    // FIXME: Make limit 0 load no items, instead of loading all items.
                    limit: MainCtx.gridView ? Math.max(newMusicRow.currentItem?.nbItemPerRow ?? null, 1) : 5
                }

                onActiveFocusChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        contentYBehavior.enabled = true
                        Helpers.positionFlickableToContainItem(flickable, item ?? this)
                        contentYBehavior.enabled = false
                    }
                }

                onCurrentIndexChanged: {
                    if (activeFocus) {
                        const item = currentItem?.currentItem ?? currentItem?._getItem(currentIndex) // FIXME: `ExpandGridView` does not have `currentItem`.
                        if (item) {
                            contentYBehavior.enabled = true
                            Helpers.positionFlickableToContainItem(flickable, item)
                            contentYBehavior.enabled = false
                        }
                    }
                }
            }
        }
    }
}
