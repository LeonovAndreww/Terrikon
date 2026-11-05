/*****************************************************************************
 * subtitle_style.cpp: quick subtitle size, background and outline
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
#include "subtitle_style.hpp"
#include "player/player_controller.hpp"

#include <vlc_configuration.h>
#include <vlc_player.h>
#include <vlc_variables.h>

#include <algorithm>

namespace terrikon {

/* The text renderer reads these at every render by inheritance, so a value
 * set on the libvlc instance reaches the subtitles of any video */
static const char backgroundVar[] = "freetype-background-opacity";
static const char outlineVar[] = "freetype-outline-thickness";

SubtitleStyle::SubtitleStyle(qt_intf_t *intf, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
{
    vlc_object_t *libvlc = VLC_OBJECT(vlc_object_instance(m_intf));
    var_Create(libvlc, backgroundVar, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create(libvlc, outlineVar, VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
}

int SubtitleStyle::textScale() const
{
    vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
    vlc_player_locker lock{ player };
    return int(vlc_player_GetSubtitleTextScale(player));
}

int SubtitleStyle::backgroundOpacity() const
{
    return int(var_GetInteger(vlc_object_instance(m_intf), backgroundVar));
}

int SubtitleStyle::outline() const
{
    return int(var_GetInteger(vlc_object_instance(m_intf), outlineVar));
}

void SubtitleStyle::setTextScale(int percent)
{
    /* The range the core accepts for "sub-text-scale" */
    percent = std::clamp(percent, 10, 500);
    if (percent == textScale())
        return;
    {
        vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
        vlc_player_locker lock{ player };
        vlc_player_SetSubtitleTextScale(player, unsigned(percent));
    }
    config_PutInt("sub-text-scale", percent);
    emit changed();
}

void SubtitleStyle::setInteger(const char *name, int value)
{
    if (var_GetInteger(vlc_object_instance(m_intf), name) == value)
        return;
    var_SetInteger(vlc_object_instance(m_intf), name, value);
    /* Saved with the rest of the settings when the player quits */
    config_PutInt(name, value);
    emit changed();
}

void SubtitleStyle::setBackgroundOpacity(int opacity)
{
    setInteger(backgroundVar, std::clamp(opacity, 0, 255));
}

void SubtitleStyle::setOutline(int thickness)
{
    setInteger(outlineVar, std::clamp(thickness, 0, 50));
}

void SubtitleStyle::reset()
{
    setTextScale(100);
    setBackgroundOpacity(0);
    setOutline(4);
}

}
