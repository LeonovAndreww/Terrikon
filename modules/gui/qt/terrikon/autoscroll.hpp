/*****************************************************************************
 * autoscroll.hpp: scrolling lists with the middle mouse button
 *****************************************************************************
 * Copyright (C) 2026 Terrikon contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef TERRIKON_AUTOSCROLL_HPP
#define TERRIKON_AUTOSCROLL_HPP

#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QTimer>
#include <QElapsedTimer>

class QGuiApplication;
class QQuickItem;
class QQuickWindow;

namespace terrikon {

/* As in web browsers: press the middle button over a list and move the
 * mouse away from that point to scroll, faster the further it goes. A
 * click without moving keeps scrolling until the next click. */
class AutoScroll : public QObject
{
    Q_OBJECT
public:
    static void install(QGuiApplication &app);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit AutoScroll(QObject *parent);
    bool start(QQuickWindow *window, const QPointF &pos);
    void stop();
    void step();

    QTimer m_timer;
    QElapsedTimer m_pressed;
    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_flickable;
    QPointF m_anchor;
    bool m_active = false;
    bool m_vertical = true;
    bool m_moved = false;
};

}

#endif
