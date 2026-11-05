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

#include <vlc_actions.h>
#include <vlc_configuration.h>
#include <vlc_tick.h>
#include <vlc_variables.h>

namespace terrikon {

int TerrikonActions::keyActionCallback(vlc_object_t *, const char *, vlc_value_t,
                                        vlc_value_t newval, void *data)
{
    const int action = int(newval.i_int);
    const bool ours = action >= ACTIONID_TERRIKON_FIRST && action <= ACTIONID_TERRIKON_LAST;
    /* Escape leaves picture in picture as it leaves full screen */
    if (!ours && action != ACTIONID_LEAVE_FULLSCREEN)
        return VLC_SUCCESS;

    auto *self = static_cast<TerrikonActions *>(data);
    QMetaObject::invokeMethod(self, [self, action] { self->run(action); }, Qt::QueuedConnection);
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
    case ACTIONID_LEAVE_FULLSCREEN:
        m_compact->leave();
        break;

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
            m_marks->toggle(nowSeconds());
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
