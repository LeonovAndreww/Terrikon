/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import VLC.Style
import VLC.Util

// targetWidth: concerned widget's current width
// HorizontalResizeHandle actually doesn't resizes target
// you have to assign target's width manually using requestedWidth property
// e.g target.width: Helpers.clamp(requestedWidth, minimumWidth, maximumWidth)
MouseArea {
    id: root

    required property int currentWidth
    property int requestedWidth: 0
    property int minimumWidth: 0
    property int maximumWidth: Number.MAX_SAFE_INTEGER

    property bool atRight: true

    // Terrikon: the panel can be folded by dragging (beyond its minimum
    // width) and unfolded the same way, as well as by its button
    property bool collapsible: false
    property bool collapsed: false
    signal collapseRequested()
    signal expandRequested()
    property bool _folded: false

    property int _previousX

    cursorShape: Qt.SplitHCursor
    width: VLCStyle.resizeHandleWidth
    acceptedButtons: Qt.LeftButton
    preventStealing: true

    onPressed: (mouse) => {
        MainCtx.setCursor(cursorShape)
        _previousX = mouseX
        _folded = false
    }

    onReleased:(mouse) => {
        MainCtx.restoreCursor()
    }

    onCanceled: {
        MainCtx.restoreCursor()
    }

    onPositionChanged: {
        const f = atRight ? 1 : -1
        const delta = mouseX - _previousX
        if (root.collapsible && !root._folded) {
            const fold = VLCStyle.dp(56, VLCStyle.scale)
            if (!root.collapsed && root.currentWidth + delta * f < minimumWidth - fold) {
                root._folded = true
                root.collapseRequested()
                return
            }
            if (root.collapsed && delta * f > fold) {
                root._folded = true
                root.expandRequested()
                return
            }
        }
        if (root.collapsed || root._folded)
            return
        root.requestedWidth = Helpers.clamp(
            root.currentWidth + (delta * f),
            minimumWidth,
            maximumWidth)
    }

}
