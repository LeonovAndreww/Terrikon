/*****************************************************************************
 * subtitle_style.hpp: quick subtitle size, background and outline
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
#ifndef TERRIKON_SUBTITLE_STYLE_HPP
#define TERRIKON_SUBTITLE_STYLE_HPP

#include <QObject>

struct qt_intf_t;

namespace terrikon {

/* Subtitle look changed from the player, applied to the subtitles on screen
 * right away and kept for the next start. Only text subtitles (SRT, plain
 * ASS lines) follow it; styled ASS keeps its own look. */
class SubtitleStyle : public QObject
{
    Q_OBJECT
    /* Percent of the normal size */
    Q_PROPERTY(int textScale READ textScale WRITE setTextScale NOTIFY changed FINAL)
    /* 0 (no box) .. 255 (opaque black box) */
    Q_PROPERTY(int backgroundOpacity READ backgroundOpacity WRITE setBackgroundOpacity NOTIFY changed FINAL)
    /* Outline thickness as the text renderer counts it: 0, 2, 4, 6 */
    Q_PROPERTY(int outline READ outline WRITE setOutline NOTIFY changed FINAL)

public:
    explicit SubtitleStyle(qt_intf_t *intf, QObject *parent = nullptr);

    int textScale() const;
    int backgroundOpacity() const;
    int outline() const;

    void setTextScale(int percent);
    void setBackgroundOpacity(int opacity);
    void setOutline(int thickness);

    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    void setInteger(const char *name, int value);

    qt_intf_t *m_intf;
};

}

#endif
