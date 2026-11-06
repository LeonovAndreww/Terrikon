/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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
import QtQml.Models

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Style

Widgets.PageLoader {
    id: root

    // Properties

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    property bool enableBeginningFade: true
    property bool enableEndFade: true

    // Settings

    pageModel: [{
        name: "base",
        default: true,
        component: componentBase
    }, {
        name: "group",
        component: componentGroup
    }, {
        name: "folder",
        component: componentFolder
    }]


    // Children

    Component {
        id: componentBase

        VideoAllSubDisplay {
            // Events

            onShowList: (model, reason) => {
                History.push([...root.pagePrefix, MainCtx.grouping === MainCtx.GROUPING_FOLDER ? "folder" : "group"],
                             { parentId: model.id, title: model.title }, reason)
            }

            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade
        }
    }

    // Terrikon: a folder, with its subfolders
    Component {
        id: componentFolder

        VideoFolderDisplay {
            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            onShowList: (model, reason) => {
                History.push([...root.pagePrefix, "folder"], { parentId: model.id, title: model.title }, reason)
            }

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade
        }
    }

    Component {
        id: componentGroup

        VideoGroupDisplay {
            onCurrentIndexChanged: History.viewProp.initialIndex = currentIndex

            function isInfoExpandPanelAvailable(/* modelIndexData */) {
                return true
            }

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            enableBeginningFade: root.enableBeginningFade
            enableEndFade: root.enableEndFade
        }
    }
}
