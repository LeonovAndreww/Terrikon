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
}

TerrikonActions::~TerrikonActions()
{
    var_DelCallback(vlc_object_instance(m_intf), "key-action", keyActionCallback, this);
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

void TerrikonActions::report(int action)
{
    /* How far a jump goes, from the same settings the core uses */
    auto jump = [this](const char *size, int sign) {
        const int seconds = int(var_InheritInteger(vlc_object_instance(m_intf), size));
        const QString amount = seconds % 60 == 0 ? qtr("%1 min").arg(seconds / 60)
                                                 : qtr("%1 s").arg(seconds);
        emit osd((sign > 0 ? QStringLiteral("+") : QStringLiteral("−")) + amount);
    };

    switch (action)
    {
    case ACTIONID_JUMP_BACKWARD_EXTRASHORT: jump("extrashort-jump-size", -1); break;
    case ACTIONID_JUMP_FORWARD_EXTRASHORT:  jump("extrashort-jump-size", +1); break;
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

    case ACTIONID_PIP_TOGGLE:
        if (player->hasVideoOutput() || m_compact->active())
            m_compact->toggle();
        break;
    }
}

}
