/*****************************************************************************
 * autoscroll.cpp: scrolling lists with the middle mouse button
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "autoscroll.hpp"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>

namespace terrikon {

static constexpr qreal deadZone = 12;

void AutoScroll::install(QGuiApplication &app)
{
    app.installEventFilter(new AutoScroll(&app));
}

AutoScroll::AutoScroll(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &AutoScroll::step);
}

static qreal number(QQuickItem *item, const char *name)
{
    return item->property(name).toReal();
}

/* How far it can scroll along one axis */
static bool scrollable(QQuickItem *f, bool vertical)
{
    if (!f->property("interactive").toBool())
        return false;
    return vertical
        ? number(f, "contentHeight") + number(f, "topMargin") + number(f, "bottomMargin") > f->height() + 1
        : number(f, "contentWidth") + number(f, "leftMargin") + number(f, "rightMargin") > f->width() + 1;
}

/* The innermost list under the point that can scroll, the topmost one when
 * several overlap. Items that only catch input over the whole window (drop
 * areas, mouse areas) are not in the way. */
static QQuickItem *flickableAt(QQuickItem *item, const QPointF &scenePos, bool vertical)
{
    QList<QQuickItem *> children = item->childItems();
    std::stable_sort(children.begin(), children.end(),
                     [](QQuickItem *a, QQuickItem *b) { return a->z() < b->z(); });
    for (auto it = children.crbegin(); it != children.crend(); ++it)
    {
        QQuickItem *child = *it;
        if (!child->isVisible() || child->opacity() <= 0.)
            continue;
        const bool inside = child->contains(child->mapFromScene(scenePos));
        if (child->clip() && !inside)
            continue;
        if (QQuickItem *found = flickableAt(child, scenePos, vertical))
            return found;
        if (inside && child->inherits("QQuickFlickable") && scrollable(child, vertical))
            return child;
    }
    return nullptr;
}

bool AutoScroll::start(QQuickWindow *window, const QPointF &pos)
{
    /* up and down first, as in a browser; sideways only where it cannot */
    bool vertical = true;
    QQuickItem *item = flickableAt(window->contentItem(), pos, true);
    if (!item)
    {
        vertical = false;
        item = flickableAt(window->contentItem(), pos, false);
    }
    if (!item)
        return false;
    m_window = window;
    m_flickable = item;
    m_vertical = vertical;
    m_anchor = pos;
    m_moved = false;
    m_active = true;
    m_pressed.start();
    QGuiApplication::setOverrideCursor(vertical ? Qt::SizeVerCursor : Qt::SizeHorCursor);
    m_timer.start();
    return true;
}

void AutoScroll::stop()
{
    if (!m_active)
        return;
    m_active = false;
    m_timer.stop();
    QGuiApplication::restoreOverrideCursor();
}

void AutoScroll::step()
{
    if (!m_window || !m_flickable || !m_flickable->isVisible())
        return stop();
    const QPointF pos = m_window->mapFromGlobal(QCursor::pos());
    const qreal d = m_vertical ? pos.y() - m_anchor.y() : pos.x() - m_anchor.x();
    if (std::abs(d) <= deadZone)
        return;
    m_moved = true;
    /* slow near the point, much faster far from it */
    const qreal speed = std::copysign(std::pow(std::abs(d) - deadZone, 1.3) * 0.06, d);

    QQuickItem *f = m_flickable;
    const char *prop = m_vertical ? "contentY" : "contentX";
    const qreal origin = number(f, m_vertical ? "originY" : "originX");
    const qreal size = number(f, m_vertical ? "contentHeight" : "contentWidth");
    const qreal view = m_vertical ? f->height() : f->width();
    const qreal low = origin - number(f, m_vertical ? "topMargin" : "leftMargin");
    const qreal high = std::max(low, origin + size - view + number(f, m_vertical ? "bottomMargin" : "rightMargin"));
    const qreal now = number(f, prop);
    const qreal next = std::clamp(now + speed, low, high);
    if (next != now)
        f->setProperty(prop, next);
}

bool AutoScroll::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type())
    {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    {
        auto window = qobject_cast<QQuickWindow *>(watched);
        if (!window)
            return false;
        auto mouse = static_cast<QMouseEvent *>(event);
        if (m_active)
        {
            /* any click ends it, and does nothing else */
            stop();
            return true;
        }
        if (mouse->button() == Qt::MiddleButton && event->type() == QEvent::MouseButtonPress)
            return start(window, mouse->scenePosition());
        return false;
    }
    case QEvent::MouseButtonRelease:
    {
        if (!m_active || !qobject_cast<QQuickWindow *>(watched))
            return false;
        auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() != Qt::MiddleButton)
            return false;
        /* held and moved: it was a drag, so it ends here; a plain click
         * keeps scrolling until the next one */
        if (m_moved || m_pressed.elapsed() > 400)
            stop();
        return true;
    }
    case QEvent::Wheel:
        if (m_active && qobject_cast<QQuickWindow *>(watched))
            stop();
        return false;
    case QEvent::KeyPress:
        if (m_active && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape)
        {
            stop();
            return true;
        }
        return false;
    case QEvent::ApplicationDeactivate:
        stop();
        return false;
    default:
        return false;
    }
}

}
