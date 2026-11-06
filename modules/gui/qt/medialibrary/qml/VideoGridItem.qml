/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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

Widgets.GridItem {
    id: root

    property bool showNewIndicator: false
    
    property var labels

    // Terrikon: the folder model, for the expand / collapse buttons
    property var treeModel: null
    property int treeRow: -1

    function play() {
        if ( model.id !== undefined ) {
            MediaLib.addAndPlay( model.id )
            MainCtx.playerView = true
        }
    }

    image: model.thumbnail || ""
    fallbackImage: VLCStyle.noArtVideoCover
    fillMode: Image.Stretch

    title: model.title || qsTr("Unknown title")
    // Terrikon: what was chosen to be written under the covers (Sort >
    // Captions). An unknown length (not parsed yet) comes as a negative
    // duration.
    subtitle: {
        const parts = []
        for (const field of MediaFolders.videoFields) {
            switch (field) {
            case "duration": {
                const text = model?.duration?.formatHMS() ?? ""
                if (text !== "" && !text.startsWith("-") && text !== "00:00")
                    parts.push(text)
                break
            }
            case "count":
                if (model?.isVideo === false && model?.count !== undefined)
                    parts.push(qsTr("%1 videos").arg(model.count))
                break
            case "size":
                if (model?.size_text)
                    parts.push(model.size_text)
                break
            case "mtime":
                if (model?.mtime_text)
                    parts.push(qsTr("changed %1").arg(model.mtime_text))
                break
            case "last_played":
                if (model?.last_played_text)
                    parts.push(qsTr("watched %1").arg(model.last_played_text))
                break
            }
        }
        return parts.join("  ·  ")
    }
    pictureWidth: VLCStyle.gridCover_video_width
    pictureHeight: VLCStyle.gridCover_video_height

    // Bottom rounding is provided by the progress bar if it is visible:
    mediaCover.radiusBottomLeft: progressBar.visible ? 0.0 : mediaCover.radius
    mediaCover.radiusBottomRight: progressBar.visible ? 0.0 : mediaCover.radius

    selectedShadow.height: selectedShadow.implicitHeight + (progressBar.visible ? progressBar.height : 0.0)
    unselectedShadow.height: unselectedShadow.implicitHeight + (progressBar.visible ? progressBar.height : 0.0)

    Widgets.VideoProgressBar {
        id: progressBar

        parent: root.mediaCover

        // If the background color of the image is opaque (which should be
        // the case by default), we can place the progress bar beneath the
        // thumbnail and disable clipping because the top part would not be
        // exposed. Note that the top part is still going to be painted, as
        // long as the image is not opaque which is the expected case since
        // SDF-based round images always have transparent parts which needs
        // blending. If the image is not rounded and its blending is disabled,
        // the top part of the bar is not going to be painted, thanks to depth
        // testing. Furthermore, fragment shader may not even be executed for
        // the top part that are obscured by the opaque image, provided that
        // early depth test is applicable. In any case, since bottom radii
        // are 0.0 and background color is opaque, the top part (if painted)
        // will not be exposed to the user.
        z: (root.mediaCover.color.a > (1.0 - Number.EPSILON)) ? -0.1 : 0.0
        clip: (z >= 0.0)

        anchors {
            top: parent.bottom
            left: parent.left
            right: parent.right
        }

        visible: (model.progress > 0)

        radius: root.pictureRadius
        value: Helpers.clamp(model.progress !== undefined ? model.progress : 0, 0, 1)
    }

    pictureOverlay: Item {
        implicitWidth: root.pictureWidth
        implicitHeight: root.pictureHeight

        Widgets.ScaledImage {
            id: image

            anchors.right: parent.right
            anchors.top: parent.top

            width: VLCStyle.gridItem_newIndicator
            height: width

            visible: root.showNewIndicator

            source: VLCStyle.newIndicator
        }

        // Terrikon: expand a folder into what it holds, collapse back; what
        // comes of it in the tooltips
        Row {
            id: treeButtons

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: VLCStyle.margin_xxsmall
            spacing: VLCStyle.margin_xxsmall

            readonly property bool _active: !!root.treeModel && root.treeRow >= 0
            visible: _active && (root.hovered || root.highlighted)

            component TreeButton: Widgets.IconToolButton {
                width: VLCStyle.dp(26, VLCStyle.scale)
                height: width
                font.pixelSize: VLCStyle.icon_small
                color: "white"
                backgroundColor: hovered ? Qt.rgba(0, 0, 0, 0.85) : Qt.rgba(0, 0, 0, 0.6)
            }

            function _index() { return root.treeModel.index(root.treeRow, 0) }

            TreeButton {
                visible: model?.canExpand === true
                text: VLCIcons.expand
                font.pixelSize: VLCStyle.icon_normal
                description: treeButtons._active && visible
                             ? qsTr("Expand: %1").arg(root.treeModel.expandPreview(treeButtons._index())) : ""
                onClicked: root.treeModel.expandItem(treeButtons._index())
            }

            TreeButton {
                visible: model?.canCollapse === true
                text: VLCIcons.expand_inverted
                font.pixelSize: VLCStyle.icon_normal
                description: treeButtons._active && visible
                             ? qsTr("Collapse into %1").arg(root.treeModel.collapseTarget(treeButtons._index())) : ""
                onClicked: root.treeModel.collapseItem(treeButtons._index())
            }

            TreeButton {
                visible: model?.canCollapseAll === true
                text: VLCIcons.expand_inverted
                font.pixelSize: VLCStyle.icon_normal
                // two chevrons: all the way up
                Widgets.IconLabel {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -VLCStyle.dp(5, VLCStyle.scale)
                    text: VLCIcons.expand_inverted
                    font.pixelSize: VLCStyle.icon_normal
                    color: "white"
                }
                description: treeButtons._active && visible
                             ? qsTr("Collapse all into %1, as when added").arg(root.treeModel.collapseAllTarget(treeButtons._index())) : ""
                onClicked: root.treeModel.collapseAllItem(treeButtons._index())
            }
        }

        Widgets.VideoQualityLabels {
            anchors {
                top: parent.top
                right: parent.right
                topMargin: VLCStyle.margin_xxsmall
                leftMargin: VLCStyle.margin_xxsmall
                rightMargin: VLCStyle.margin_xxsmall
            }

            labels: root.labels
        }
    }

    onPlayClicked: root.play()
}
