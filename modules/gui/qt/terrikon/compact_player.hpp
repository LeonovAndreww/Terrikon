/*****************************************************************************
 * compact_player.hpp: picture in picture for the main window
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
#ifndef TERRIKON_COMPACT_PLAYER_HPP
#define TERRIKON_COMPACT_PLAYER_HPP

#include <QObject>
#include <QRect>
#include <QSize>
#include <QWindow>

class MainCtx;

namespace terrikon {

/* Picture in picture: the main window shrinks to a small always-on-top
 * video in a corner of the screen, without the interface around it, and
 * comes back as it was. The same window is reused rather than a second one
 * opened, so the video output does not have to move. */
class CompactPlayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged FINAL)

public:
    /* Share of the screen width the video takes */
    static constexpr double screenShare = 0.25;
    static constexpr int minimumWidth = 320;
    static constexpr int screenMargin = 24;
    /* The main window refuses to get smaller than its usual minimum */
    static constexpr int minimumHeight = 90;

    explicit CompactPlayer(MainCtx *ctx, QObject *parent = nullptr);
    ~CompactPlayer() override;

    static CompactPlayer *instance() { return s_instance; }

    bool active() const { return m_active; }

    Q_INVOKABLE void toggle();
    Q_INVOKABLE void enter();
    Q_INVOKABLE void leave();

signals:
    void activeChanged();

private:
    static CompactPlayer *s_instance;
    MainCtx *m_ctx;
    bool m_active = false;

    /* What leave() puts back */
    QRect m_geometry;
    QSize m_minimumSize;
    QWindow::Visibility m_visibility = QWindow::Windowed;
    bool m_minimalView = false;
    bool m_onTop = false;
};

}

#endif
