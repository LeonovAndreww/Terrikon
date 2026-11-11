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
import VLC.Terrikon

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

    // Terrikon: a hue from a folder key, the same each time
    function _groupHue(key) {
        let h = 0
        for (let i = 0; i < key.length; ++i)
            h = (h * 31 + key.charCodeAt(i)) % 360
        return h / 360
    }

    // Terrikon: brought in by expanding a folder: comes in gently
    Component.onCompleted: {
        if ((model.group ?? "") !== "")
            appear.start()
    }
    ParallelAnimation {
        id: appear
        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "scale"; from: 0.94; to: 1; duration: 220; easing.type: Easing.OutCubic }
    }

    function play() {
        if ( model.id !== undefined ) {
            // Terrikon: a video plays with the rest of its folder after it
            if ( model.isVideo === false )
                MediaLib.addAndPlay( model.id )
            else
                MediaLib.playWithFolder( model.id )
            MainCtx.playerView = true
        }
    }

    image: model.thumbnail || ""

    // Terrikon: no big play button over a playing preview
    property bool _previewShowing: false
    playCoverShowPlay: implicitPlayCoverShowPlay && !_previewShowing
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

        // Terrikon: what an expanded folder brought in is marked with a colour
        // of its own (the folder's), along the top of the picture
        Rectangle {
            readonly property string group: model.group ?? ""
            visible: group !== ""
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: VLCStyle.dp(4, VLCStyle.scale)
            radius: height / 2
            color: Qt.hsla(root._groupHue(group), 0.65, 0.55, 1)
        }

        // Terrikon: resting the pointer on a video plays it in the tile, as
        // video sites preview theirs: muted unless asked, a seek bar with a
        // frame over the pointer, the time left. Opening the video then
        // starts where the preview was; it does not count as watching.
        Timer {
            id: previewDelay
            property bool armed: false
            readonly property bool wanted: root.hovered && model.isVideo !== false && !!model.mrl
            onWantedChanged: if (!wanted) armed = false
            // repeats so that firing does not end the binding of running
            interval: 500
            repeat: true
            running: wanted && !armed
            onTriggered: armed = true
        }

        Loader {
            id: previewLoader
            anchors.fill: parent
            active: previewDelay.armed
            sourceComponent: Item {
                id: preview

                readonly property bool showing: video.hasFrame
                onShowingChanged: root._previewShowing = showing
                Component.onDestruction: root._previewShowing = false

                function _time(seconds) {
                    const s = Math.max(0, Math.floor(seconds))
                    const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, r = s % 60
                    const two = (n) => (n < 10 ? "0" : "") + n
                    return h > 0 ? h + ":" + two(m) + ":" + two(r) : m + ":" + two(r)
                }

                Rectangle {
                    anchors.fill: parent
                    color: "black"
                    visible: video.hasFrame
                    radius: root.pictureRadius
                }

                VideoPreview {
                    id: video
                    anchors.fill: parent
                    source: model.mrl
                }

                // sound on / off, remembered for every preview
                Rectangle {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.margins: VLCStyle.margin_xsmall
                    width: VLCStyle.dp(30, VLCStyle.scale)
                    height: width
                    radius: width / 2
                    visible: video.hasFrame
                    color: muteArea.containsMouse ? Qt.rgba(0, 0, 0, 0.85) : Qt.rgba(0, 0, 0, 0.6)

                    Widgets.IconLabel {
                        anchors.centerIn: parent
                        text: video.muted ? VLCIcons.volume_muted : VLCIcons.volume_high
                        color: "white"
                        font.pixelSize: VLCStyle.icon_small
                    }

                    MouseArea {
                        id: muteArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: video.muted = !video.muted
                    }
                }

                // time left
                Rectangle {
                    anchors.right: parent.right
                    anchors.bottom: bar.top
                    anchors.margins: VLCStyle.margin_xsmall
                    visible: video.hasFrame && video.length > 0
                    width: left.implicitWidth + VLCStyle.margin_xsmall * 2
                    height: left.implicitHeight + VLCStyle.margin_xxxsmall * 2
                    radius: VLCStyle.dp(3, VLCStyle.scale)
                    color: Qt.rgba(0, 0, 0, 0.7)

                    Text {
                        id: left
                        anchors.centerIn: parent
                        color: "white"
                        font.pixelSize: VLCStyle.fontSize_small
                        font.bold: true
                        text: preview._time(video.length - video.time)
                    }
                }

                // seek bar: thin, thicker under the pointer, with the frame
                // of the moment above it
                Item {
                    id: bar
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: VLCStyle.margin_xsmall
                    anchors.rightMargin: VLCStyle.margin_xsmall
                    height: VLCStyle.dp(22, VLCStyle.scale)
                    visible: video.hasFrame

                    readonly property real hoverFraction: barArea.containsMouse
                        ? Math.max(0, Math.min(1, barArea.mouseX / width)) : -1

                    Rectangle {
                        id: track
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        height: barArea.containsMouse ? VLCStyle.dp(5, VLCStyle.scale) : VLCStyle.dp(3, VLCStyle.scale)
                        radius: height / 2
                        color: Qt.rgba(1, 1, 1, 0.35)

                        Rectangle {
                            width: parent.width * video.position
                            height: parent.height
                            radius: parent.radius
                            color: "#FF2D2D"
                        }
                    }

                    Rectangle {
                        visible: barArea.containsMouse
                        x: parent.width * video.position - width / 2
                        anchors.verticalCenter: track.verticalCenter
                        width: VLCStyle.dp(11, VLCStyle.scale)
                        height: width
                        radius: width / 2
                        color: "#FF2D2D"
                    }

                    MouseArea {
                        id: barArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPressed: (mouse) => video.seek(mouse.x / width)
                        onPositionChanged: (mouse) => { if (pressed) video.seek(mouse.x / width) }
                    }

                    ThumbnailStrip {
                        id: strip
                        keyFramesOnly: true
                        Component.onCompleted: loadUrl(model.mrl)
                    }

                    // the frame over the pointer, with its time
                    Column {
                        visible: bar.hoverFraction >= 0 && video.length > 0
                        readonly property real at: bar.hoverFraction * video.length
                        readonly property var frame: strip.ready ? strip.frameAt(at) : ({})
                        x: Math.max(0, Math.min(bar.width - width, bar.hoverFraction * bar.width - width / 2))
                        anchors.bottom: track.top
                        anchors.bottomMargin: VLCStyle.margin_xsmall
                        spacing: VLCStyle.margin_xxxsmall

                        Rectangle {
                            visible: !!parent.frame.source
                            width: VLCStyle.dp(128, VLCStyle.scale)
                            height: width * 9 / 16
                            radius: VLCStyle.dp(6, VLCStyle.scale)
                            color: "black"
                            border.color: "white"
                            border.width: VLCStyle.dp(2, VLCStyle.scale)
                            clip: true

                            Image {
                                anchors.fill: parent
                                anchors.margins: parent.border.width
                                source: parent.parent.frame.source ?? ""
                                sourceClipRect: parent.parent.frame.source
                                    ? Qt.rect(parent.parent.frame.x, parent.parent.frame.y,
                                              parent.parent.frame.width, parent.parent.frame.height)
                                    : undefined
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: true
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            color: "white"
                            style: Text.Outline
                            styleColor: Qt.rgba(0, 0, 0, 0.8)
                            font.pixelSize: VLCStyle.fontSize_normal
                            font.bold: true
                            text: preview._time(parent.at)
                        }
                    }
                }
            }
        }

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

            // A dark chip with a white chevron (or two), clear on any cover
            component TreeButton: Rectangle {
                id: treeButton

                property string description
                property bool twoChevrons: false
                property bool up: false
                signal clicked()

                width: VLCStyle.dp(30, VLCStyle.scale)
                height: VLCStyle.dp(24, VLCStyle.scale)
                radius: VLCStyle.dp(6, VLCStyle.scale)
                color: buttonHover.hovered ? Qt.rgba(0, 0, 0, 0.9) : Qt.rgba(0, 0, 0, 0.7)
                border.color: Qt.rgba(1, 1, 1, buttonHover.hovered ? 0.5 : 0.2)

                Widgets.IconLabel {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: treeButton.twoChevrons ? VLCStyle.dp(3, VLCStyle.scale) : 0
                    text: treeButton.up ? VLCIcons.expand_inverted : VLCIcons.expand
                    font.pixelSize: VLCStyle.dp(26, VLCStyle.scale)
                    color: "white"
                }

                Widgets.IconLabel {
                    visible: treeButton.twoChevrons
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -VLCStyle.dp(3, VLCStyle.scale)
                    text: VLCIcons.expand_inverted
                    font.pixelSize: VLCStyle.dp(26, VLCStyle.scale)
                    color: "white"
                }

                HoverHandler { id: buttonHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: treeButton.clicked() }

                Widgets.ToolTipExt {

                    colorContext.palette: VLCStyle.palette
                    visible: buttonHover.hovered && treeButton.description !== ""
                    text: treeButton.description
                }
            }

            function _index() { return root.treeModel.index(root.treeRow, 0) }

            TreeButton {
                visible: model?.canExpand === true
                description: treeButtons._active && visible
                             ? qsTr("Expand: %1").arg(root.treeModel.expandPreview(treeButtons._index())) : ""
                onClicked: root.treeModel.expandItem(treeButtons._index())
            }

            TreeButton {
                visible: model?.canCollapse === true
                up: true
                description: treeButtons._active && visible
                             ? qsTr("Collapse into %1").arg(root.treeModel.collapseTarget(treeButtons._index())) : ""
                onClicked: root.treeModel.collapseItem(treeButtons._index())
            }

            TreeButton {
                visible: model?.canCollapseAll === true
                up: true
                twoChevrons: true
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
