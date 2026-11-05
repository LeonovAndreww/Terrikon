/*****************************************************************************
 * Copyright (C) 2026 Terrikon authors
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
#ifndef TERRIKON_WINDOW_THEME_HPP
#define TERRIKON_WINDOW_THEME_HPP

#include <QObject>

class QApplication;
struct qt_intf_t;

namespace terrikon {

/* The widget windows (preferences, about, media information...) dark or
 * light like the player: their title bar, drawn by Windows, and, with the
 * Fusion style, the windows themselves. */
class WindowTheme : public QObject
{
public:
    WindowTheme(qt_intf_t *intf, QObject *parent);

    /* Whether the player is dark: its colour scheme, or the system's
     * when it follows the system */
    static bool isDark(qt_intf_t *intf);

    /* Fusion with a dark palette when the player is dark (unless the user
     * picked another style), and a dark title bar on every window */
    static void install(qt_intf_t *intf, QApplication &app, bool changeStyle);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    qt_intf_t *m_intf;
};

}

#endif
