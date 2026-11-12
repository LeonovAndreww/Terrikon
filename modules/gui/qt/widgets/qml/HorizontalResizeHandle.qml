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
    // where the drag began, on the screen, and the width then: the panel
    // moves under the pointer while folding, its own coordinates with it
    property real _pressGlobalX: 0
    property int _pressWidth: 0

    property int _previousX

    cursorShape: Qt.SplitHCursor
    width: VLCStyle.resizeHandleWidth
    acceptedButtons: Qt.LeftButton
    preventStealing: true

    onPressed: (mouse) => {
        MainCtx.setCursor(cursorShape)
        _previousX = mouseX
        _pressGlobalX = mapToGlobal(mouseX, 0).x
        _pressWidth = currentWidth
    }

    onReleased:(mouse) => {
        MainCtx.restoreCursor()
    }

    onCanceled: {
        MainCtx.restoreCursor()
    }

    onPositionChanged: {
        const f = atRight ? 1 : -1
        if (!root.collapsible) {
            const delta = mouseX - _previousX
            root.requestedWidth = Helpers.clamp(root.currentWidth + (delta * f), minimumWidth, maximumWidth)
            return
        }
        // the width the pointer asks for, from where the drag began: folding
        // and unfolding happen within the same drag, both ways
        const wanted = _pressWidth + (mapToGlobal(mouseX, 0).x - _pressGlobalX) * f
        const fold = VLCStyle.dp(48, VLCStyle.scale)
        if (!root.collapsed && wanted < minimumWidth - fold) {
            root.collapseRequested()
            return
        }
        if (root.collapsed) {
            if (wanted > minimumWidth - fold / 2)
                root.expandRequested()
            else
                return
        }
        root.requestedWidth = Helpers.clamp(wanted, minimumWidth, maximumWidth)
    }

}
