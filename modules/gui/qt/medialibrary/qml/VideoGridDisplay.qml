/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Leon Vitanos <leon.vitanos@gmail.com>
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

import VLC.MediaLibrary

import VLC.MainInterface
import VLC.Util
import VLC.Widgets as Widgets
import VLC.Style

Widgets.ExpandGridItemView {
    id: gridView

    // Properties

    property MLDragItem dragItem
    property MLContextMenu contextMenu
    property var labels

    // Signals

    signal itemDoubleClicked(var model)

    // Settings

    basePictureWidth: VLCStyle.gridCover_video_width
    basePictureHeight: VLCStyle.gridCover_video_height

    activeFocusOnTab: true

    // Functions

    // reimplement function to show "Info Panel" in grid view for the model index data
    function isInfoExpandPanelAvailable(modelIndexData) {
        return true
    }

    // Connections

    Connections {
        target: gridView.contextMenu

        function onShowMediaInformation(index) {
            gridView.switchExpandItem(index)

            if (gridView.focus && expandItem)
                expandItem.setCurrentItemFocus(Qt.TabFocusReason)
        }
    }

    // Terrikon: the cards brought in from one folder, side by side, on one
    // background of the folder's colour: a few rectangles per folder for the
    // rows in view (the cards themselves draw nothing then)
    readonly property MLVideoFoldersModel _treeModel: (model instanceof MLVideoFoldersModel) ? model : null
    property var _runs: []

    function _updateRuns() {
        if (!_treeModel || nbItemPerRow <= 0) {
            _runs = []
            return
        }
        // a row more on each side: the ends of a folder out of view stay so
        const first = Math.max(_currentRange[0] - nbItemPerRow, 0)
        const last = Math.min(_currentRange[1] + nbItemPerRow, _count)
        const runs = []
        let run = null
        for (let i = first; i < last; ++i) {
            const key = _treeModel.tintKeyAt(i)
            if (key === "") {
                run = null
            } else if (run && run.key === key) {
                run.end = i
            } else {
                run = { key: key, start: i, end: i }
                runs.push(run)
            }
        }
        _runs = runs
    }

    function _groupHue(key) {
        let h = 0
        for (let i = 0; i < key.length; ++i)
            h = (h * 31 + key.charCodeAt(i)) % 360
        return h / 360
    }

    on_CurrentRangeChanged: Qt.callLater(_updateRuns)
    onNbItemPerRowChanged: Qt.callLater(_updateRuns)
    on_CountChanged: Qt.callLater(_updateRuns)

    Connections {
        target: gridView._treeModel
        function onDataChanged() { Qt.callLater(gridView._updateRuns) }
        function onModelReset() { Qt.callLater(gridView._updateRuns) }
        function onLayoutChanged() { Qt.callLater(gridView._updateRuns) }
    }

    underlay: Item {
        id: underlay

        // the pieces of each run: its first row, the full rows, its last
        // row, and the gaps between them where cards are above and below
        readonly property var pieces: {
            const out = []
            const c = gridView.nbItemPerRow
            if (c <= 0)
                return out
            const p = VLCStyle.gridItemSelectedBorder
            const w = gridView.cellWidth, h = gridView.cellHeight
            const split = gridView.expandShiftFrom()
            const shift = gridView._expandItemVerticalSpace
            const X = (col) => gridView.getItemPos(col)[0]
            const Y = (row) => gridView.getItemPos(row * c)[1] + ((split >= 0 && row * c >= split) ? shift : 0)
            const box = (c0, c1, y0, y1, color, tl, tr, bl, br) => out.push({
                x: X(c0) - p, y: y0, w: X(c1) + w + p - (X(c0) - p), h: y1 - y0,
                color: color, tl: tl, tr: tr, bl: bl, br: br })

            for (const whole of gridView._runs) {
                // apart on both sides of an open information panel
                const parts = (split > whole.start && split <= whole.end)
                            ? [[whole.start, split - 1], [split, whole.end]]
                            : [[whole.start, whole.end]]
                const color = Qt.hsla(gridView._groupHue(whole.key), 0.6, 0.55, 0.13)
                for (const part of parts) {
                    const rs = Math.floor(part[0] / c), cs = part[0] % c
                    const re = Math.floor(part[1] / c), ce = part[1] % c
                    const top = (row) => Y(row) - p, bottom = (row) => Y(row) + h + p
                    if (rs === re) {
                        box(cs, ce, top(rs), bottom(rs), color, true, true, true, true)
                        continue
                    }
                    const middle = re - rs > 1
                    // the gap under the first row: where a card is below too
                    const g1 = middle ? [cs, c - 1] : (ce >= cs ? [cs, ce] : null)
                    box(cs, c - 1, top(rs), bottom(rs), color, true, true,
                        g1 === null, g1 === null || g1[1] !== c - 1)
                    if (g1)
                        box(g1[0], g1[1], bottom(rs), top(rs + 1), color, false, false, false, false)
                    let above = g1
                    if (middle) {
                        box(0, c - 1, top(rs + 1), bottom(re - 1), color,
                            cs !== 0, false, false, ce !== c - 1)
                        box(0, ce, bottom(re - 1), top(re), color, false, false, false, false)
                        above = [0, ce]
                    }
                    box(0, ce, top(re), bottom(re), color,
                        above === null || above[0] !== 0, above === null || above[1] !== ce,
                        true, true)
                }
            }
            return out
        }

        Repeater {
            model: underlay.pieces.length

            Rectangle {
                required property int index
                readonly property var piece: underlay.pieces[index] ?? null
                readonly property real r: VLCStyle.dp(8, VLCStyle.scale)

                x: piece?.x ?? 0
                y: piece?.y ?? 0
                width: piece?.w ?? 0
                height: piece?.h ?? 0
                color: piece?.color ?? "transparent"
                topLeftRadius: piece?.tl ? r : 0
                topRightRadius: piece?.tr ? r : 0
                bottomLeftRadius: piece?.bl ? r : 0
                bottomRightRadius: piece?.br ? r : 0
            }
        }
    }

    // Children

    delegate: VideoGridItem {
        id: gridItem

        // Properties

        required property var model
        required property int index

        // Settings

        width: gridView.cellWidth
        height: gridView.cellHeight

        pictureWidth: gridView.maxPictureWidth
        pictureHeight: gridView.maxPictureHeight

        opacity: (gridView.expandIndex !== -1
                  &&
                  gridView.expandIndex !== gridItem.index) ? 0.7 : 1

        // FIXME: Sometimes MLBaseModel::getDataAt returns {} so we use 'isNew === true'.
        showNewIndicator: (model.isNew === true)

        dragItem: gridView.dragItem

        labels: gridView.labels(model)

        treeModel: (gridView.model instanceof MLVideoFoldersModel) ? gridView.model : null
        treeRow: index

        // Events

        onItemClicked: (modifier, select) => { gridView.leftClickOnItem(modifier, index, select) }

        onItemDoubleClicked: gridView.itemDoubleClicked(model)

        onContextMenuButtonClicked: (_, globalMousePos) => {
            gridView.rightClickOnItem(index);

            const options = {}
            if (gridView.isInfoExpandPanelAvailable(model))
                options["information"] = index

            gridView.contextMenu.popup(selectionModel.selectedIndexes, globalMousePos, options);
        }

        // Animations

        Behavior on opacity {
            // NOTE: Do not use `OpacityAnimator` here, interrupting the fade stalls the UI (QTBUG-151700).
            NumberAnimation {
                duration: VLCStyle.duration_short
            }
        }
    }

    expandDelegate: VideoInfoExpandPanel {
        x: 0

        width: gridView.width

        Navigation.parentItem: gridView

        Navigation.cancelAction: gridView.forceFocus
        Navigation.upAction: gridView.forceFocus
        Navigation.downAction: gridView.forceFocus

        onRetract: gridView.retract()
    }

    function forceFocus() {
        setCurrentItemFocus(Qt.TabFocus)
    }
}
