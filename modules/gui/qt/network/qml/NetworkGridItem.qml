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
import QtQml.Models

import VLC.Widgets as Widgets
import VLC.Network
import VLC.Style
import VLC.Util
import VLC.Terrikon

Widgets.GridItem {
    id: root

    property var model: ({})
    property int index: -1

    pictureWidth: VLCStyle.gridCover_network_width
    pictureHeight: VLCStyle.gridCover_network_height

    playCoverShowPlay: {
        switch (model.type) {
            case NetworkMediaModel.TYPE_NODE:
            case NetworkMediaModel.TYPE_DIRECTORY:
            case NetworkMediaModel.TYPE_PLAYLIST:
                return false
            default:
                return implicitPlayCoverShowPlay
        }
    }

    image: {
        if (model.artwork && model.artwork.toString() !== "") {
            // through VLC's own access, as the list view does: Qt alone
            // does not fetch http(s) (country flags, station logos)
            return VLCAccessImage.uri(model.artwork)
        }
        return ""
    }

    fillMode: Image.PreserveAspectCrop

    // Terrikon: a folder with a picture of its own (a country's flag, a
    // square image with clear bands above and below it) sits on a soft card,
    // not on the near black of an empty cover
    Binding {
        target: root.mediaCover
        property: "color"
        when: (model.type === NetworkMediaModel.TYPE_NODE || model.type === NetworkMediaModel.TYPE_DIRECTORY)
              && !!model.artwork && model.artwork.toString() !== ""
        value: Qt.alpha(root.colorContext.fg.primary, 0.08)
    }

    // Terrikon: a stream (a radio station) can be kept among the favourites,
    // shown on the home page
    readonly property bool _stream: {
        const mrl = model.mrl?.toString() ?? ""
        return model.type !== NetworkMediaModel.TYPE_DIRECTORY && model.type !== NetworkMediaModel.TYPE_NODE
               && /^(https?|mms|rtsp|icyx?):/i.test(mrl)
    }
    property bool _favourite: _stream && MediaFolders.isFavourite(model.mrl.toString())
    Connections {
        target: MediaFolders
        function onFavouritesChanged() { root._favourite = root._stream && MediaFolders.isFavourite(model.mrl.toString()) }
    }

    cacheImage: true // we may have network thumbnail

    fallbackImage: {
        return SVGColorImage.colorize(model.artworkFallback)
                            .color1(root.colorContext.fg.primary)
                            .accent(root.colorContext.accent)
                            .uri()
    }

    title: model.name || qsTr("Unknown share")
    subtitle: {
        // make sure subtitle is never empty otherwise it causes alignment issues
        const defaultTxt = "--"

        const mrl = model.mrl?.toString() || ""
        const type = model.type

        if ((type === NetworkMediaModel.TYPE_NODE
            || type === NetworkMediaModel.TYPE_DIRECTORY
            || type === NetworkMediaModel.TYPE_PLAYLIST)
            && (mrl === "vlc://nop"))
            return defaultTxt

        return mrl || defaultTxt
    }

    pictureOverlay: Item {
        implicitWidth: root.pictureWidth
        implicitHeight: root.pictureHeight

        Widgets.VideoProgressBar {
            id: progressBar

            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }

            visible: (model.progress ?? - 1) > 0

            radius: root.pictureRadius
            value:  visible
                    ? Helpers.clamp(model.progress, 0, 1)
                    : 0
        }

        Rectangle {
            visible: root._stream && (root.hovered || root._favourite)
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: VLCStyle.margin_xsmall
            width: VLCStyle.dp(28, VLCStyle.scale)
            height: width
            radius: width / 2
            color: heartHover.hovered ? Qt.rgba(0, 0, 0, 0.75) : Qt.rgba(0, 0, 0, 0.5)

            Text {
                anchors.centerIn: parent
                text: root._favourite ? "\u2665" : "\u2661"
                color: root._favourite ? "#FF4F6D" : "white"
                font.pixelSize: VLCStyle.dp(17, VLCStyle.scale)
            }
            HoverHandler { id: heartHover; cursorShape: VLCStyle.pointerCursor }
            TapHandler {
                onTapped: MediaFolders.toggleFavourite(model.name || "", model.mrl.toString(),
                                                       model.artwork ? model.artwork.toString() : "")
            }
        }
    }
}
