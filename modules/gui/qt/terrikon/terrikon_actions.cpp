/*****************************************************************************
 * terrikon_actions.cpp: Terrikon hotkey actions
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include "terrikon_actions.hpp"
#include "clip_selection.hpp"
#include "compact_player.hpp"
#include "moment_marks.hpp"
#include "player/player_controller.hpp"
#include "media_folders.hpp"
#include "explorer_order.hpp"
#include "media_tools.hpp"

#include <QCollator>
#include <atomic>
#include <QDir>
#include <QFileInfo>
#include <vlc_playlist.h>
#include <vlc_interface.h>
#include <vlc_input_item.h>

#include <QTimer>

#include <vlc_actions.h>
#include <vlc_player.h>
#include <vlc_configuration.h>
#include <vlc_tick.h>
#include <vlc_variables.h>

namespace terrikon {

int TerrikonActions::keyActionCallback(vlc_object_t *, const char *, vlc_value_t,
                                        vlc_value_t newval, void *data)
{
    const int action = int(newval.i_int);
    auto *self = static_cast<TerrikonActions *>(data);
    if (action >= ACTIONID_TERRIKON_FIRST && action <= ACTIONID_TERRIKON_LAST)
        QMetaObject::invokeMethod(self, [self, action] { self->run(action); }, Qt::QueuedConnection);
    else
        /* After the hotkeys module has acted on it */
        QMetaObject::invokeMethod(self, [self, action] {
            QTimer::singleShot(150, self, [self, action] { self->report(action); });
        }, Qt::QueuedConnection);
    return VLC_SUCCESS;
}

TerrikonActions::TerrikonActions(qt_intf_t *intf, ClipSelection *clip, MomentMarks *marks,
                                 CompactPlayer *compact, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
    , m_clip(clip)
    , m_marks(marks)
    , m_compact(compact)
{
    var_AddCallback(vlc_object_instance(m_intf), "key-action", keyActionCallback, this);

    static const struct vlc_playlist_callbacks cbs = [] {
        struct vlc_playlist_callbacks c {};
        c.on_items_added = &TerrikonActions::onItemsAdded;
        c.on_items_reset = &TerrikonActions::onItemsReset;
        return c;
    }();
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(m_intf->intf);
    vlc_playlist_Lock(playlist);
    m_playlistListener = vlc_playlist_AddListener(playlist, &cbs, this, true);
    vlc_playlist_Unlock(playlist);
}

TerrikonActions::~TerrikonActions()
{
    var_DelCallback(vlc_object_instance(m_intf), "key-action", keyActionCallback, this);
    if (m_playlistListener)
    {
        vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(m_intf->intf);
        vlc_playlist_Lock(playlist);
        vlc_playlist_RemoveListener(playlist, m_playlistListener);
        vlc_playlist_Unlock(playlist);
    }
}

/* playlist locked, any thread */
static std::atomic<int> libraryAppends{0};
LibraryAppend::LibraryAppend() { ++libraryAppends; }
LibraryAppend::~LibraryAppend() { --libraryAppends; }
bool LibraryAppend::active() { return libraryAppends.load() > 0; }

void TerrikonActions::onItemsAdded(vlc_playlist_t *playlist, size_t,
                                   vlc_playlist_item_t *const items[], size_t count,
                                   void *data)
{
    if (count != 1 || vlc_playlist_Count(playlist) != 1 || LibraryAppend::active())
        return;
    input_item_t *media = vlc_playlist_item_GetMedia(items[0]);
    if (!media || !media->psz_uri || strncmp(media->psz_uri, "file://", 7) != 0)
        return;
    const QString mrl = QString::fromUtf8(media->psz_uri);
    auto self = static_cast<TerrikonActions *>(data);
    QMetaObject::invokeMethod(self, [self, mrl] { self->queueFolderOf(mrl); }, Qt::QueuedConnection);
}

void TerrikonActions::onItemsReset(vlc_playlist_t *playlist, vlc_playlist_item_t *const items[],
                                   size_t count, void *data)
{
    onItemsAdded(playlist, 0, items, count, data);
}

static QString localKey(const QString &mrl)
{
    return QDir::cleanPath(QUrl(mrl).toLocalFile()).toLower();
}

void TerrikonActions::queueFolderOf(const QString &mrl)
{
    struct Found { QStringList before, after; };
    /* the folder may be on a drive slow to answer: listed aside */
    offThread(this, [mrl] {
        Found found;
        const QFileInfo file(QUrl(mrl).toLocalFile());
        if (!file.exists())
            return found;
        const QString suffix = QStringLiteral("*.") + file.suffix().toLower();
        const QStringList &patterns = audioPatterns().contains(suffix) ? audioPatterns()
                                                                       : videoPatterns();
        if (!patterns.contains(suffix))
            return found;
        QStringList names = file.dir().entryList(patterns, QDir::Files | QDir::Readable);
        if (names.size() < 2 || names.size() > 2000)
            return found;
        const auto indexOf = [](const QStringList &list, const QString &name) {
            for (int i = 0; i < list.size(); ++i)
                if (list[i].compare(name, Qt::CaseInsensitive) == 0)
                    return i;
            return -1;
        };
        /* the order of an explorer window showing the folder, as sorted
         * and grouped there; else by names as the explorer does at first:
         * numbers as numbers, case left aside */
        QStringList shown;
        for (const QString &path : terrikon::explorerOrder(file.absolutePath()))
        {
            const QFileInfo item(path);
            if (indexOf(names, item.fileName()) >= 0 && indexOf(shown, item.fileName()) < 0)
                shown << item.fileName();
        }
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        if (indexOf(shown, file.fileName()) >= 0)
        {
            /* the files the window did not list (filtered out, or an answer
             * cut short) are queued after its own, by name */
            QStringList rest;
            for (const QString &name : names)
                if (indexOf(shown, name) < 0)
                    rest << name;
            std::sort(rest.begin(), rest.end(), collator);
            names = shown + rest;
        }
        else
            std::sort(names.begin(), names.end(), collator);
        const int at = indexOf(names, file.fileName());
        if (at < 0)
            return found;
        for (int i = 0; i < names.size(); ++i)
        {
            if (i == at)
                continue;
            const QString url = QUrl::fromLocalFile(file.dir().filePath(names[i])).toString(QUrl::FullyEncoded);
            (i < at ? found.before : found.after) << url;
        }
        return found;
    }, [this, mrl](const Found &found) {
        if (found.before.isEmpty() && found.after.isEmpty())
            return;
        vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(m_intf->intf);
        vlc_playlist_Lock(playlist);
        /* still that file alone: nothing else was asked meanwhile */
        input_item_t *media = vlc_playlist_Count(playlist) == 1
            ? vlc_playlist_item_GetMedia(vlc_playlist_Get(playlist, 0)) : nullptr;
        if (media && media->psz_uri && localKey(QString::fromUtf8(media->psz_uri)) == localKey(mrl))
        {
            const auto insert = [playlist](size_t index, const QStringList &urls) {
                std::vector<input_item_t *> items;
                for (const QString &url : urls)
                    if (input_item_t *item = input_item_New(qtu(url), nullptr))
                        items.push_back(item);
                if (!items.empty())
                    vlc_playlist_Insert(playlist, index, items.data(), items.size());
                for (input_item_t *item : items)
                    input_item_Release(item);
            };
            insert(0, found.before);
            insert(vlc_playlist_Count(playlist), found.after);
        }
        vlc_playlist_Unlock(playlist);
    });
}

QString TerrikonActions::keyFor(const QString &action) const
{
    char *key = config_GetPsz(qtu(QStringLiteral("key-") + action));
    const QString text = key ? QString::fromUtf8(key) : QString();
    free(key);
    return text;
}

QString TerrikonActions::hint(const QString &action) const
{
    QString best;
    const QStringList keys = keyFor(action).split(QLatin1Char('\t'), Qt::SkipEmptyParts);
    for (const QString &key : keys)
    {
        if (key.startsWith(QLatin1String("Media ")) || key.startsWith(QLatin1String("Browser "))
            || key.startsWith(QLatin1String("Volume ")))
            continue;
        if (best.isEmpty() || key.size() < best.size())
            best = key;
    }
    if (best.isEmpty())
        return {};
    /* "Alt+x" reads better as "Alt+X"; a lone letter stays as typed */
    const qsizetype plus = best.lastIndexOf(QLatin1Char('+'));
    if (plus >= 0 && plus == best.size() - 2)
        best[best.size() - 1] = best.back().toUpper();
    return QStringLiteral(" (%1)").arg(best);
}

QSizeF TerrikonActions::videoSize() const
{
    vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
    vlc_player_locker lock{ player };
    const struct vlc_player_track *track = vlc_player_GetSelectedTrack(player, VIDEO_ES);
    if (!track)
        return {};
    const video_format_t &fmt = track->fmt.video;
    /* Demuxers often fill only the coded size in the track format */
    double width = fmt.i_visible_width ? fmt.i_visible_width : fmt.i_width;
    const double height = fmt.i_visible_height ? fmt.i_visible_height : fmt.i_height;
    if (width <= 0. || height <= 0.)
        return {};
    if (fmt.i_sar_num && fmt.i_sar_den)
        width = width * fmt.i_sar_num / fmt.i_sar_den;
    return QSizeF(width, height);
}

QString TerrikonActions::selectedTrackName(int category) const
{
    vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
    vlc_player_locker lock{ player };
    QStringList names;
    const size_t count = vlc_player_GetTrackCount(player, es_format_category_e(category));
    for (size_t i = 0; i < count; ++i)
    {
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt(player, es_format_category_e(category), i);
        if (track && track->selected)
            names << qfu(track->name);
    }
    return names.join(QStringLiteral(", "));
}

void TerrikonActions::skip(bool forward)
{
    const int seconds = int(var_InheritInteger(vlc_object_instance(m_intf), "short-jump-size"));
    if (forward)
        m_intf->p_mainPlayerController->jumpFwd();
    else
        m_intf->p_mainPlayerController->jumpBwd();
    emit jumped(forward ? seconds : -seconds);
}

void TerrikonActions::report(int action)
{
    /* How far a jump goes, from the same settings the core uses */
    auto jump = [this](const char *size, int sign) {
        const int seconds = int(var_InheritInteger(vlc_object_instance(m_intf), size));
        emit jumped(sign * seconds);
    };

    switch (action)
    {
    case ACTIONID_JUMP_BACKWARD_EXTRASHORT: jump("extrashort-jump-size", -1); break;
    case ACTIONID_JUMP_FORWARD_EXTRASHORT:  jump("extrashort-jump-size", +1); break;
    /* Arrows without a disc menu seek by the extra short step */
    case ACTIONID_NAV_LEFT:                 jump("extrashort-jump-size", -1); break;
    case ACTIONID_NAV_RIGHT:                jump("extrashort-jump-size", +1); break;
    case ACTIONID_JUMP_BACKWARD_SHORT:      jump("short-jump-size", -1); break;
    case ACTIONID_JUMP_FORWARD_SHORT:       jump("short-jump-size", +1); break;
    case ACTIONID_JUMP_BACKWARD_MEDIUM:     jump("medium-jump-size", -1); break;
    case ACTIONID_JUMP_FORWARD_MEDIUM:      jump("medium-jump-size", +1); break;
    case ACTIONID_JUMP_BACKWARD_LONG:       jump("long-jump-size", -1); break;
    case ACTIONID_JUMP_FORWARD_LONG:        jump("long-jump-size", +1); break;
    case ACTIONID_FRAME_NEXT:               emit osd(qtr("Next frame")); break;
    case ACTIONID_FRAME_PREVIOUS:           emit osd(qtr("Previous frame")); break;
    case ACTIONID_SUBTITLE_TOGGLE:
    case ACTIONID_SUBTITLE_TRACK:
    case ACTIONID_SUBTITLE_REVERSE_TRACK:
    {
        const QString name = selectedTrackName(SPU_ES);
        emit osd(name.isEmpty() ? qtr("Subtitles off") : qtr("Subtitles: %1").arg(name));
        break;
    }
    case ACTIONID_AUDIO_TRACK:
        emit osd(qtr("Audio: %1").arg(selectedTrackName(AUDIO_ES)));
        break;
    case ACTIONID_LEAVE_FULLSCREEN:
        /* Esc in the small picture-in-picture window goes back to the player */
        if (m_compact->active())
            m_compact->leave();
        break;
    default:
        break;
    }
}

double TerrikonActions::lengthSeconds() const
{
    return secf_from_vlc_tick(m_intf->p_mainPlayerController->getLength().toVLCTick());
}

double TerrikonActions::nowSeconds() const
{
    return m_intf->p_mainPlayerController->getPosition() * lengthSeconds();
}

void TerrikonActions::run(int action)
{
    PlayerController *player = m_intf->p_mainPlayerController;
    const double length = lengthSeconds();

    switch (action)
    {
    case ACTIONID_CLIP_TOGGLE:
        if (m_clip->active())
            m_clip->close();
        else if (player->isStarted() && player->isSeekable() && length > 0.)
            m_clip->begin(nowSeconds(), length);
        break;
    case ACTIONID_CLIP_START:
        if (m_clip->active())
            m_clip->setStart(nowSeconds());
        break;
    case ACTIONID_CLIP_END:
        if (m_clip->active())
            m_clip->setEnd(nowSeconds());
        break;
    case ACTIONID_CLIP_EXPORT:
        if (m_clip->active())
            emit clipExportRequested();
        break;

    case ACTIONID_MARK_TOGGLE:
        if (player->isStarted())
        {
            const int before = m_marks->count();
            const double now = nowSeconds();
            m_marks->toggle(now);
            emit osd(m_marks->count() > before ? qtr("Marked %1").arg(m_clip->formatTime(now))
                                               : qtr("Mark removed"));
        }
        break;
    case ACTIONID_MARK_NEXT:
    case ACTIONID_MARK_PREV:
    {
        const double target = action == ACTIONID_MARK_NEXT ? m_marks->next(nowSeconds())
                                                           : m_marks->previous(nowSeconds());
        if (target >= 0. && length > 0.)
            player->setPosition(target / length);
        break;
    }

    case ACTIONID_STATS_TOGGLE:
        emit statsToggleRequested();
        break;

    case ACTIONID_PIP_TOGGLE:
        if (player->hasVideoOutput() || m_compact->active())
            m_compact->toggle();
        break;
    }
}

}
