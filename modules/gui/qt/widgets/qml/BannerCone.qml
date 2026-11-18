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
import VLC.Widgets as Widgets
import VLC.Style
import VLC.Util
import VLC.Dialogs
import QtQuick.Templates as T

Image {
    id: root

    required property color color

    // Terrikon: as a site's logo, it leads to the home page (from the player
    // too), wherever it is shown
    property bool leadsHome: true

    sourceSize: Qt.size(VLCStyle.icon_normal, VLCStyle.icon_normal)

    source: SVGColorImage.colorize("qrc:///misc/cone.svg").accent(root.color).uri()

    focus: false

    HoverHandler {
        id: coneHover
        enabled: root.leadsHome
        cursorShape: VLCStyle.pointerCursor
    }

    T.ToolTip.visible: root.leadsHome && coneHover.hovered && !aboutCard.opened
    T.ToolTip.delay: VLCStyle.delayToolTipAppear
    T.ToolTip.text: qsTr("Home")

    // the logo lies on the title bar: its clicks are its own, not the
    // title bar's too (a double click maximized, a right click opened the
    // window's menu over the card)
    TapHandler {
        enabled: root.leadsHome
        gesturePolicy: TapHandler.ReleaseWithinBounds
        grabPermissions: PointerHandler.TakeOverForbidden
        onTapped: {
            MainCtx.playerView = false
            if (!History.match(History.viewPath, ["home"]))
                History.push(["home"])
        }
    }

    // Terrikon: a right click: what this is, who made it, where it lives
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        grabPermissions: PointerHandler.TakeOverForbidden
        onTapped: aboutCard.open()
    }

    T.Popup {
        id: aboutCard

        y: root.height + VLCStyle.margin_xxsmall
        width: VLCStyle.dp(280, VLCStyle.scale)
        implicitHeight: implicitContentHeight + topPadding + bottomPadding
        padding: VLCStyle.margin_small
        closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

        readonly property ColorContext colorContext: ColorContext {
            id: cardTheme
            colorSet: ColorContext.Window
            // a popup has no item above it to take the palette from
            palette: VLCStyle.palette
        }

        background: Rectangle {
            radius: VLCStyle.dp(12, VLCStyle.scale)
            color: cardTheme.bg.primary
            border.color: cardTheme.border
            border.width: 1
        }

        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 } }

        component Entry: Rectangle {
            id: entry
            property string label
            signal picked()
            width: parent.width
            height: VLCStyle.dp(32, VLCStyle.scale)
            radius: VLCStyle.dp(6, VLCStyle.scale)
            color: entryHover.hovered ? Qt.alpha(cardTheme.fg.primary, 0.08) : "transparent"
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: VLCStyle.margin_small
                text: entry.label
                color: cardTheme.fg.primary
                font.pixelSize: VLCStyle.fontSize_normal
            }
            HoverHandler { id: entryHover; cursorShape: VLCStyle.pointerCursor }
            TapHandler {
                onTapped: {
                    aboutCard.close()
                    entry.picked()
                }
            }
        }

        contentItem: Column {
            spacing: VLCStyle.margin_xxxsmall

            Row {
                spacing: VLCStyle.margin_small
                bottomPadding: VLCStyle.margin_xsmall

                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    sourceSize: Qt.size(VLCStyle.icon_large, VLCStyle.icon_large)
                    source: SVGColorImage.colorize("qrc:///misc/cone.svg").accent(root.color).uri()
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "Terrikon"
                        color: cardTheme.fg.primary
                        font.pixelSize: VLCStyle.fontSize_large
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("Based on VLC %1").arg(AboutModel.version)
                        color: cardTheme.fg.secondary
                        font.pixelSize: VLCStyle.fontSize_small
                    }
                }
            }

            Entry {
                label: qsTr("About, authors and licence")
                onPicked: DialogsProvider.aboutDialog()
            }
            Entry {
                label: qsTr("The project on GitHub")
                onPicked: Qt.openUrlExternally("https://github.com/LeonovAndreww/Terrikon")
            }
            Entry {
                label: qsTr("Report a problem")
                onPicked: Qt.openUrlExternally("https://github.com/LeonovAndreww/Terrikon/issues/new")
            }
            Entry {
                label: qsTr("Messages and errors")
                onPicked: DialogsProvider.messagesDialog()
            }
        }
    }
}
