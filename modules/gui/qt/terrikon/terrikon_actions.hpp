/*****************************************************************************
 * terrikon_actions.hpp: Terrikon hotkey actions
 *****************************************************************************
 * Copyright (C) 2026 Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#ifndef TERRIKON_TERRIKON_ACTIONS_HPP
#define TERRIKON_TERRIKON_ACTIONS_HPP

#include "qt.hpp"

#include <QObject>
#include <vlc_playlist.h>
#include <QSizeF>
#include <QString>

namespace terrikon {

/* While it lives, what is added to the playlist comes from the library (a
 * song of an artist, a video of a folder...): its folder is not queued
 * around it, as for a file opened from the explorer */
struct LibraryAppend
{
    LibraryAppend();
    ~LibraryAppend();
    static bool active();
};

class ClipSelection;
class CompactPlayer;
class MomentMarks;

/* Runs the Terrikon actions (clip, marks, picture in picture) when their
 * keys are pressed. The keys are VLC hotkeys like any other, set in
 * Preferences > Hotkeys; the core turns a key into an action and this
 * object, listening to "key-action", carries it out whatever view is
 * shown. */
class TerrikonActions : public QObject
{
    Q_OBJECT

public:
    TerrikonActions(qt_intf_t *intf, ClipSelection *clip, MomentMarks *marks,
                    CompactPlayer *compact, QObject *parent = nullptr);
    ~TerrikonActions() override;

    /* The key currently bound to a Terrikon action ("clip-toggle"...),
     * as written in the preferences, empty if none */
    Q_INVOKABLE QString keyFor(const QString &action) const;
    /* " (k)" for a tool tip: the plainest key bound to any VLC action
     * ("play-pause", "toggle-fullscreen", "clip-toggle"...), media
     * keys left out; empty when there is none */
    Q_INVOKABLE QString hint(const QString &action) const;
    /* Displayed size of the playing video (pixel aspect applied), empty
     * without video */
    Q_INVOKABLE QSizeF videoSize() const;

signals:
    /* The clip panel owns the export settings, so it does the export */
    void clipExportRequested();
    /* A short message to show over the video for a key press: how far a
     * jump went, which track is on, a mark set... */
    void osd(const QString &text);
    /* A jump by a key: shown on the side it goes, in seconds (negative
     * backwards) */
    void jumped(int seconds);

private:
    /* Called on the thread that pressed the key */
    static int keyActionCallback(vlc_object_t *, const char *, vlc_value_t,
                                 vlc_value_t newval, void *data);
    void run(int action);
    /* Messages for VLC's own actions the interface reports on */
    void report(int action);
    QString selectedTrackName(int category) const;
    double nowSeconds() const;
    double lengthSeconds() const;

    /* A single local file played (opened from the explorer, say): the other
     * files of its folder are queued around it, in the explorer's order,
     * so the next and previous buttons go through the folder */
    static void onItemsAdded(vlc_playlist_t *, size_t index,
                             vlc_playlist_item_t *const items[], size_t count,
                             void *data);
    /* the file given on the command line is there before the interface */
    static void onItemsReset(vlc_playlist_t *, vlc_playlist_item_t *const items[],
                             size_t count, void *data);
    void queueFolderOf(const QString &mrl);
    struct vlc_playlist_listener_id *m_playlistListener = nullptr;

    qt_intf_t *m_intf;
    ClipSelection *m_clip;
    MomentMarks *m_marks;
    CompactPlayer *m_compact;
};

}

#endif
