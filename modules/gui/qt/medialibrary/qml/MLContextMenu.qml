
/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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
import QtQml

import VLC.MainInterface
import VLC.MediaLibrary
import VLC.Dialogs

import VLC.Util
import VLC.Terrikon

// @brief - a generic ML context menu
NativeMenu {
    id: root

    // Properties

    required property MLBaseModel model

    property string idDataRole: "id"
    property string albumIdDataRole: "album_id"

    property bool showPlayAsAudioAction: false

    // This is only applicable when the model is `MLAudioModel`:
    property bool showPlayAsAlbumAction: implicitShowPlayAsAlbumAction
    property bool implicitShowPlayAsAlbumAction: !showPlayAsAudioAction

    // Signals

    signal showMediaInformation(int index)


    // required by TableViewExt
    property var tableView_popup: function (index/* reserved for future */, selectedIndexes, globalPos) {
        return popup(selectedIndexes, globalPos)
    }

    // Settings

    actions: [{
            "text": qsTr("Play"),
            "icon": ":/menu/ic_fluent_play_20_regular.svg",
            "action": addAndPlay
        }, {
            "text": qsTr("Expand"),
            "icon": ":/menu/ic_fluent_chevron_down_20_regular.svg",
            "action": (dataList, options, indexes) => model.expandItem(indexes[0]),
            "visible": (dataList, options, indexes) => _checkRole(dataList, "canExpand", true)
        }, {
            "text": qsTr("Collapse"),
            "icon": ":/menu/ic_fluent_chevron_up_20_regular.svg",
            "action": (dataList, options, indexes) => model.collapseItem(indexes[0]),
            "visible": (dataList, options, indexes) => _checkRole(dataList, "canCollapse", true)
        }, {
            "text": qsTr("Collapse all"),
            "icon": ":/menu/ic_fluent_chevron_double_up_20_regular.svg",
            "action": (dataList, options, indexes) => model.collapseAllItem(indexes[0]),
            "visible": (dataList, options, indexes) => _checkRole(dataList, "canCollapseAll", true)
        }, {
            "text": qsTr("Play as audio"),
            "icon": ":/menu/ic_fluent_music_note_2_20_regular.svg",
            "action": playAsAudio,
            "visible": root.showPlayAsAudioAction
        }, {
            "text": qsTr("Play as album"),
            "action": playAsAlbum,
            "visible": root.showPlayAsAlbumAction &&
                       (root.model instanceof MLAudioModel)
        }, {
            "text": qsTr("Enqueue"),
            "icon": ":/menu/ic_fluent_text_bullet_list_add_20_regular.svg",
            "action": enqueue
        }, {
            "text": qsTr("Enqueue as album"),
            "action": enqueueAsAlbum,
            "visible": root.showPlayAsAlbumAction &&
                       (root.model instanceof MLAudioModel)
        }, {
            "text": qsTr("Add to favorites"),
            "icon": ":/menu/ic_fluent_star_20_regular.svg",
            "action": addFavorite,
            "visible": _showAddFavorite
        }, {
            "text": qsTr("Remove from favorites"),
            "icon": ":/menu/ic_fluent_star_off_20_regular.svg",
            "action": removeFavorite,
            "visible": _showRemoveFavorite
        }, {
            "text": (root.showPlayAsAudioAction ? qsTr("Add to a video-only playlist") : qsTr("Add to an audio-only playlist")),
            "icon": ":/menu/ic_fluent_add_square_20_regular.svg",
            "action": (root.showPlayAsAudioAction ? root.addToVideoOnlyPlaylist : root.addToAudioOnlyPlaylist)
        }, {
            "text": qsTr("Add to a playlist"),
            "icon": ":/menu/ic_fluent_add_square_20_regular.svg",
            "action": addToAPlaylist
        }, {
            "text": qsTr("Mark as seen"),
            "icon": ":/menu/ic_fluent_eye_20_regular.svg",
            "action": markSeen,
            "visible": _showSeen
        }, {
            "text": qsTr("Mark as unseen"),
            "icon": ":/menu/ic_fluent_eye_off_20_regular.svg",
            "action": markUnseen,
            "visible": _showUnseen
        }, {
            "text": qsTr("Open Containing Folder"),
            "icon": ":/menu/ic_fluent_folder_open_20_regular.svg",
            "action": openContainingFolder,
            "visible": _openContainingFolder
        }, {
            "text": qsTr("Copy"),
            "icon": ":/menu/ic_fluent_copy_20_regular.svg",
            "action": (dataList, options, indexes) => MediaFolders.copyFiles(dataList.map((d) => d.mrl)),
            "visible": (dataList, options, indexes) => dataList.some((d) => String(d.mrl ?? "").startsWith("file:"))
        }, {
            "text": qsTr("Delete"),
            "icon": ":/menu/ic_fluent_delete_20_regular.svg",
            "action": deleteStream,
            "visible": _deleteStream
        }, {
            "text": qsTr("Information"),
            "icon": ":/menu/ic_fluent_info_20_regular.svg",
            "action": _signalShowInformation,
            "visible": showInformationAvailable
        },{
            "text": qsTr("Delete File From System"),
            "icon": ":/menu/ic_fluent_delete_20_regular.svg",
            "action": deleteFileFromSource,
            "visible": _deleteFileFromSource
        },{
            "text": qsTr("Media Information"),
            "icon": ":/menu/ic_fluent_info_20_regular.svg",
            "action": function(dataList, options, indexes) {
                DialogsProvider.mediaInfoDialog(dataList[0][idDataRole])
            },
            "visible": function(dataList, options, indexes) {
                return (dataList.length === 1)
                        && !(dataList[0][idDataRole].hasParent())
                        && !showInformationAvailable(dataList, options, indexes)
            }
        }
    ]

    // Events

    onRequestData: (requestID, indexes) => {
        model.getData(indexes, function (data) {
            setData(requestID, data)
        })
    }

    // Functions

    function addAndPlay(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList), _playerOptions(options))
    }

    function playAsAudio(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList), _playerOptions(options, ":no-video"))
    }

    function playAsAlbum(dataList, options, indexes) {
        model.ml.addAndPlay(_mlIDList(dataList, root.albumIdDataRole), _playerOptions(options))
    }

    function enqueue(dataList, options, indexes) {
        model.ml.addToPlaylist(_mlIDList(dataList), _playerOptions(options))
    }

    function enqueueAsAlbum(dataList, options, indexes) {
        model.ml.addToPlaylist(_mlIDList(dataList, root.albumIdDataRole), _playerOptions(options))
    }

    function addToAudioOnlyPlaylist(dataList, options, indexes) {
        addToAPlaylist(dataList, options, indexes, MLPlaylistListModel.PLAYLIST_TYPE_AUDIO_ONLY)
    }

    function addToVideoOnlyPlaylist(dataList, options, indexes) {
        addToAPlaylist(dataList, options, indexes, MLPlaylistListModel.PLAYLIST_TYPE_VIDEO_ONLY)
    }

    function addToAPlaylist(dataList, options, indexes, type) {
        if (type === undefined)
            type = MLPlaylistListModel.PLAYLIST_TYPE_ALL

        DialogsProvider.playlistsDialog(_mlIDList(dataList), type)
    }

    function addFavorite(dataList, options, indexes) {
        model.setMediaIsFavorite(indexes[0], true)
    }

    function removeFavorite(dataList, options, indexes) {
        model.setMediaIsFavorite(indexes[0], false)
    }

    function markSeen(dataList, options, indexes) {
        model.setItemPlayed(indexes[0], true)
    }

    function markUnseen(dataList, options, indexes) {
        model.setItemPlayed(indexes[0], false)
    }

    function openContainingFolder(dataList, options, indexes) {
        // Terrikon: the file selected in its folder, as the explorer does
        const mrl = String(dataList[0]?.mrl ?? "")
        if (mrl.startsWith("file:")) {
            MediaFolders.revealFile(mrl)
            return
        }
        const parentDir = model.getParentURL(indexes[0]);

        Qt.openUrlExternally(parentDir)
    }

    function deleteStream(dataList, options, indexes) {
        model.deleteStream(dataList[0][idDataRole])
    }

    function deleteFileFromSource(dataList, options, indexes) {
        let confirm = DialogsProvider.questionDialog(qsTr("Are you sure you want to delete this file?"));
        
        if (confirm) {
            model.deleteFileFromSource(indexes[0]);
            console.log("File Deleted !!");
        }
    }

    function showInformationAvailable(dataList, options, indexes) {
        return indexes.length === 1
                && Helpers.isInteger(options?.["information"] ?? null)
    }

    // Private

    function _checkRole(dataList, role, expected) {
        if (dataList.length !== 1)
            return false

        if (!(role in dataList[0]))
            return false

        return (dataList[0][role] === expected)
    }

    function _showAddFavorite(dataList, options, indexes) {
        return _checkRole(dataList, "isFavorite", false)
    }

    function _showRemoveFavorite(dataList, options, indexes) {
        return _checkRole(dataList, "isFavorite", true)
    }

    function _showSeen(dataList, options, indexes) {
        return _checkRole(dataList, "isNew", true)
    }

    function _showUnseen(dataList, options, indexes) {
        return _checkRole(dataList, "isNew", false)
    }

    function _openContainingFolder(dataList, options, indexes) {
        return _checkRole(dataList, "isLocal", true)
    }

    function _deleteStream(dataList,options, indexes) {
        return _checkRole(dataList, "isDeletable", true)
    }

    function _deleteFileFromSource(dataList, options, indexes) {
        return  _checkRole(dataList, "isDeletableFile", true)
    }

    function _signalShowInformation(dataList, options) {
        const index = options?.["information"] ?? null
        console.assert(Helpers.isInteger(index))
        showMediaInformation(index)
    }

    function _playerOptions(options, extraOptions) {
        const playerOpts = options?.["player-options"] ?? []
        return playerOpts.concat(extraOptions)
    }

    function _mlIDList(dataList, role = root.idDataRole) {
        const idList = []
        for (let i in dataList) {
            console.assert(dataList[i][role] !== undefined)
            idList.push(dataList[i][role])
        }

        return idList
    }
}
