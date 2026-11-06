/*****************************************************************************
 * compact_player.cpp: picture in picture for the main window
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
#include "compact_player.hpp"
#include "maininterface/mainctx.hpp"

#include <QCoreApplication>
#include <QScreen>
#ifdef _WIN32
# include <QLibrary>
# include <windows.h>
#endif

#include <algorithm>

namespace terrikon {

CompactPlayer *CompactPlayer::s_instance = nullptr;

CompactPlayer::~CompactPlayer()
{
    if (s_instance == this)
        s_instance = nullptr;
}

CompactPlayer::CompactPlayer(MainCtx *ctx, QObject *parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    s_instance = this;
    /* The window size is saved once the event loop is over; quitting from
     * picture in picture must not make the small window the remembered one */
    connect(qApp, &QCoreApplication::aboutToQuit, this, &CompactPlayer::leave);
}

void CompactPlayer::toggle()
{
    if (m_active)
        leave();
    else
        enter();
}

/* Rounded or square corners for the small window. Only Windows 11 rounds
 * window corners; earlier versions ignore the request and stay square. */
static void setWindowCorners(QWindow *window, int preference)
{
#ifdef _WIN32
    using SetAttribute = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
    static const auto setAttribute = reinterpret_cast<SetAttribute>(
        QLibrary::resolve(QStringLiteral("dwmapi"), "DwmSetWindowAttribute"));
    if (!setAttribute || !window)
        return;
    const DWORD cornerPreference = 33; /* DWMWA_WINDOW_CORNER_PREFERENCE */
    setAttribute(reinterpret_cast<HWND>(window->winId()), cornerPreference,
                 &preference, sizeof(preference));
#else
    Q_UNUSED(window);
    Q_UNUSED(preference);
#endif
}

enum { CORNERS_DEFAULT = 0, CORNERS_SQUARE = 1, CORNERS_ROUND = 2 };

void CompactPlayer::enter()
{
    QWindow *window = m_ctx->intfMainWindow();
    if (m_active || !window)
        return;

    m_geometry = window->geometry();
    m_visibility = window->visibility();
    m_minimalView = m_ctx->isMinimalView();
    m_onTop = m_ctx->isInterfaceAlwaysOnTop();

    /* A maximized or full screen window ignores a new geometry. Full screen
     * goes through the interface so its own state follows. */
    if (m_visibility == QWindow::FullScreen)
        emit m_ctx->setInterfaceFullScreen(false);
    if (m_visibility == QWindow::Maximized || m_visibility == QWindow::FullScreen)
    {
        if (window->visibility() != QWindow::Windowed)
            window->showNormal();
        /* showNormal() brings back the size from before maximizing, which is
         * the one to restore later */
        m_geometry = window->geometry();
    }

    QScreen *screen = window->screen();
    const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 720);
    const int width = std::max(minimumWidth, int(area.width() * screenShare));
    const int height = width * 9 / 16;
    m_minimumSize = window->minimumSize();
    window->setMinimumSize(QSize(minimumWidth / 2, minimumHeight));
    window->setGeometry(area.right() - screenMargin - width,
                        area.bottom() - screenMargin - height,
                        width, height);

    m_ctx->setMinimalView(true);
    m_ctx->setInterfaceAlwaysOnTop(true);
    setWindowCorners(window, var_InheritBool(m_ctx->getIntf(), "qt-pip-rounded")
                             ? CORNERS_ROUND : CORNERS_SQUARE);
    /* Keys (arrows, space, frame steps) must reach it right away */
    window->requestActivate();

    m_active = true;
    emit activeChanged();
}

void CompactPlayer::leave()
{
    QWindow *window = m_ctx->intfMainWindow();
    if (!m_active || !window)
        return;

    m_ctx->setInterfaceAlwaysOnTop(m_onTop);
    m_ctx->setMinimalView(m_minimalView);
    setWindowCorners(window, CORNERS_DEFAULT);

    window->setMinimumSize(m_minimumSize);
    window->setGeometry(m_geometry);
    if (m_visibility == QWindow::Maximized)
        window->showMaximized();
    else if (m_visibility == QWindow::FullScreen)
        emit m_ctx->setInterfaceFullScreen(true);

    m_active = false;
    emit activeChanged();
}

}
