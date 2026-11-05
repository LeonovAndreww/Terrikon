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
import QtQuick.Layouts

import VLC.Style
import VLC.Widgets as Widgets
import VLC.Terrikon

// Size, background and outline of subtitles in a few clicks, the way web
// players offer them, instead of the preferences dialog
ColumnLayout {
    id: root

    required property ColorContext colorContext
    property bool showTitle: true

    spacing: VLCStyle.margin_xsmall

    // One labelled row of choices; "values" and "labels" go together
    component ChoiceRow: RowLayout {
        id: row

        property string title
        property var values: []
        property var labels: []
        property int current: 0
        property color textColor: "white"
        signal chosen(int value)

        Layout.fillWidth: true
        spacing: VLCStyle.margin_xxsmall

        Widgets.ListLabel {
            Layout.preferredWidth: VLCStyle.dp(110, VLCStyle.scale)
            text: row.title
            color: row.textColor
        }

        Repeater {
            model: row.values.length

            Widgets.TextToolButton {
                id: choice

                required property int index
                readonly property bool current: row.values[index] === row.current

                text: row.labels[index]
                font.bold: current
                onClicked: row.chosen(row.values[index])

                // The chosen value is underlined
                Rectangle {
                    visible: choice.current
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: choice.leftPadding
                    anchors.rightMargin: choice.rightPadding
                    height: VLCStyle.dp(2, VLCStyle.scale)
                    radius: height / 2
                    color: row.textColor
                }
            }
        }

        Item { Layout.fillWidth: true }
    }

    Widgets.SubtitleLabel {
        visible: root.showTitle
        Layout.fillWidth: true
        text: qsTr("Subtitle style")
        color: root.colorContext.fg.primary
    }

    ChoiceRow {
        textColor: root.colorContext.fg.primary
        title: qsTr("Size")
        values: [ 75, 100, 125, 150, 200 ]
        labels: [ "75%", "100%", "125%", "150%", "200%" ]
        current: SubtitleStyle.textScale
        onChosen: (value) => SubtitleStyle.textScale = value
    }

    ChoiceRow {
        textColor: root.colorContext.fg.primary
        title: qsTr("Background")
        values: [ 0, 128, 255 ]
        labels: [ qsTr("None"), qsTr("Half"), qsTr("Solid") ]
        current: SubtitleStyle.backgroundOpacity
        onChosen: (value) => SubtitleStyle.backgroundOpacity = value
    }

    ChoiceRow {
        textColor: root.colorContext.fg.primary
        title: qsTr("Outline")
        values: [ 0, 2, 4, 6 ]
        labels: [ qsTr("None"), qsTr("Thin"), qsTr("Normal"), qsTr("Thick") ]
        current: SubtitleStyle.outline
        onChosen: (value) => SubtitleStyle.outline = value
    }

    Widgets.TextToolButton {
        text: qsTr("Reset style")
        onClicked: SubtitleStyle.reset()
    }
}
