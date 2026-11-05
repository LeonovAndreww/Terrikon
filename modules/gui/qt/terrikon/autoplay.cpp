/*****************************************************************************
 * autoplay.cpp: what happens when a video ends
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

#include "autoplay.hpp"
#include "player/player_controller.hpp"
#include "playlist/playlist_controller.hpp"

#include <QSettings>

#include <vlc_player.h>

namespace terrikon {

static const char settingsKey[] = "Terrikon/autoplay";

Autoplay::Autoplay(qt_intf_t *intf, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
{
    m_enabled = m_intf->mainSettings->value(QString::fromLatin1(settingsKey), true).toBool();

    /* Whether there is a next video changes with the queue, the current
     * video and the repeat mode; hasNext covers all of them */
    connect(m_intf->p_mainPlaylistController, &vlc::playlist::PlaylistController::hasNextChanged,
            this, &Autoplay::apply);
    apply();
}

void Autoplay::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    m_intf->mainSettings->setValue(QString::fromLatin1(settingsKey), enabled);
    apply();
    emit enabledChanged();
}

void Autoplay::apply()
{
    const bool goOn = m_enabled && m_intf->p_mainPlaylistController->hasNext();
    vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
    vlc_player_locker lock{ player };
    vlc_player_SetPlayAndPause(player, !goOn);
}

}
