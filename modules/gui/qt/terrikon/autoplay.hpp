/*****************************************************************************
 * autoplay.hpp: what happens when a video ends
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
#ifndef TERRIKON_AUTOPLAY_HPP
#define TERRIKON_AUTOPLAY_HPP

#include "qt.hpp"

#include <QObject>

namespace terrikon {

/* At the end of a video, go on with the next one in the queue only when
 * autoplay is on and there is one; otherwise stay paused on the last frame
 * rather than closing the video, so it can still be looked at, clipped or
 * replayed. */
class Autoplay : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged FINAL)

public:
    explicit Autoplay(qt_intf_t *intf, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

signals:
    void enabledChanged();

private:
    /* Tells the player whether to pause at the end of the current video */
    void apply();

    qt_intf_t *m_intf;
    bool m_enabled = true;
};

}

#endif
