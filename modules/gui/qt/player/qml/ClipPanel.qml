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

import VLC.Player
import VLC.Style
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Terrikon

// Clip mode strip of the control bar: the chosen range, how to export it
// and the export itself. The range is edited on the seek bar.
FocusScope {
    id: root

    // Discord's upload limit for accounts without Nitro is 10 MB; aim a bit
    // lower so container overhead never tips it over
    readonly property real discordBytes: 9.5 * 1024 * 1024

    readonly property bool _running: exporter.running
    readonly property int _enabledTracks: {
        // Re-read when tracks are switched or their volume changes
        void Player.audioTracks.count
        void _tracksRevision
        return Player.audioTracks.trackStates().filter(t => t.enabled).length
    }
    property int _tracksRevision: 0

    // Last export result, cleared when the range changes
    property string _resultPath: ""
    property string _error: ""

    implicitHeight: groups.implicitHeight + 2 * VLCStyle.margin_xsmall
    visible: ClipSelection.active

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    function _currentSeconds() {
        return Player.position * ClipSelection.mediaLength
    }

    function exportClip() {
        _resultPath = ""
        _error = ""

        const preset = ["copy", "quality", "size"][presetBox.currentIndex]
        const params = {
            input: Player.url.toString(),
            start: ClipSelection.start,
            end: ClipSelection.end,
            tracks: Player.audioTracks.trackStates(),
            merge: mergeBox.checked && root._enabledTracks > 1,
            preset: preset,
            vertical: verticalBox.checked,
            cropX: 0.5,
            copyToClipboard: clipboardBox.checked
        }
        if (preset === "size")
            params.targetBytes = root.discordBytes

        if (!exporter.start(params))
            _error = qsTr("Could not start the export.")
    }

    Connections {
        target: TerrikonActions
        function onClipExportRequested() {
            if (!root._running)
                root.exportClip()
        }
    }

    ClipExporter {
        id: exporter

        onFinished: (ok, output, error) => {
            if (ok) {
                root._resultPath = output
                root._error = ""
            } else {
                root._resultPath = ""
                root._error = error
            }
        }
    }

    Connections {
        target: ClipSelection
        function onRangeChanged() {
            root._resultPath = ""
            root._error = ""
        }
    }

    Connections {
        target: Player.audioTracks
        function onDataChanged() { root._tracksRevision++ }
    }

    // The strip lies above the control bar background, over the video
    Rectangle {
        anchors.fill: parent
        radius: VLCStyle.dp(8, VLCStyle.scale)
        color: theme.bg.primary
        opacity: 0.9
    }

    // The range and the export settings: side by side when the window is
    // wide enough, the export settings on a second line otherwise
    Flow {
        id: groups

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: VLCStyle.margin_xsmall
        anchors.rightMargin: VLCStyle.margin_xsmall

        spacing: VLCStyle.margin_large

    RowLayout {
        spacing: VLCStyle.margin_small

        Widgets.IconToolButton {
            text: VLCIcons.close
            description: qsTr("Close clip mode")
            enabled: !root._running
            onClicked: ClipSelection.close()
        }

        Widgets.TextToolButton {
            id: startButton
            text: qsTr("Start here")
            Widgets.ToolTipExt {
                visible: startButton.hovered
                text: TerrikonActions.keyFor("clip-start")
            }
            enabled: !root._running
            onClicked: ClipSelection.start = root._currentSeconds()
        }

        Text {
            text: qsTr("%1 – %2 (%3 s)")
                    .arg(ClipSelection.formatTime(ClipSelection.start))
                    .arg(ClipSelection.formatTime(ClipSelection.end))
                    .arg(ClipSelection.duration.toFixed(1))
            color: theme.fg.primary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.TextToolButton {
            id: endButton
            text: qsTr("End here")
            Widgets.ToolTipExt {
                visible: endButton.hovered
                text: TerrikonActions.keyFor("clip-end")
            }
            enabled: !root._running
            onClicked: ClipSelection.end = root._currentSeconds()
        }

    }

    RowLayout {
        spacing: VLCStyle.margin_small

        Widgets.CheckBoxExt {
            id: mergeBox
            text: qsTr("Mix tracks")
            checked: true
            visible: root._enabledTracks > 1
            enabled: !root._running
        }

        Widgets.CheckBoxExt {
            id: verticalBox
            text: qsTr("Vertical")
            enabled: !root._running
        }

        Widgets.ComboBoxExt {
            id: presetBox

            Layout.preferredWidth: VLCStyle.dp(170, VLCStyle.scale)

            model: [ qsTr("Original, fast"), qsTr("Re-encode"), qsTr("Discord (10 MB)") ]
            enabled: !root._running
        }

        Widgets.CheckBoxExt {
            id: clipboardBox
            text: qsTr("Copy to clipboard")
            checked: true
            enabled: !root._running
        }

        Widgets.ProgressBarExt {
            visible: root._running
            Layout.preferredWidth: VLCStyle.dp(120, VLCStyle.scale)
            from: 0
            to: 1
            value: exporter.progress
        }

        Text {
            visible: !root._running && (root._resultPath !== "" || root._error !== "")
            Layout.maximumWidth: VLCStyle.dp(260, VLCStyle.scale)
            elide: Text.ElideMiddle

            text: root._error !== ""
                  ? root._error
                  : (clipboardBox.checked ? qsTr("Saved and copied") : qsTr("Saved"))
            color: root._error !== "" ? theme.fg.negative : theme.fg.secondary
            font.pixelSize: VLCStyle.fontSize_normal
        }

        Widgets.TextToolButton {
            visible: !root._running && root._resultPath !== ""
            text: qsTr("Show")
            onClicked: exporter.reveal(root._resultPath)
        }

        Widgets.ButtonExt {
            text: root._running ? qsTr("Cancel") : qsTr("Export")
            onClicked: {
                if (root._running)
                    exporter.cancel()
                else
                    root.exportClip()
            }
        }
    }
    }
}
