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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "window_theme.hpp"

#include "qt.hpp"
#include "maininterface/mainctx.hpp"
#include "util/color_scheme_model.hpp"

#include <QApplication>
#include <QEvent>
#include <QLibrary>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QWidget>

#ifdef _WIN32
# include <windows.h>
#endif

namespace terrikon {

WindowTheme::WindowTheme(qt_intf_t *intf, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
{
}

bool WindowTheme::isDark(qt_intf_t *intf)
{
    if (intf && intf->p_mi && intf->p_mi->getColorScheme())
    {
        switch (intf->p_mi->getColorScheme()->currentScheme())
        {
        case ColorSchemeModel::Night: return true;
        case ColorSchemeModel::Day: return false;
        case ColorSchemeModel::System: break;
        }
    }
#ifdef _WIN32
    const QSettings personalize(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    return personalize.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0;
#else
    return false;
#endif
}

static QPalette darkPalette()
{
    const QColor text(0xee, 0xee, 0xee);
    const QColor dimmed(0x6f, 0x6f, 0x6f);
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x1f, 0x1f, 0x1f));
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, QColor(0x17, 0x17, 0x17));
    p.setColor(QPalette::AlternateBase, QColor(0x26, 0x26, 0x26));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, QColor(0x2c, 0x2c, 0x2c));
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, QColor(0x3a, 0x3a, 0x3a));
    p.setColor(QPalette::Midlight, QColor(0x33, 0x33, 0x33));
    p.setColor(QPalette::Mid, QColor(0x2a, 0x2a, 0x2a));
    p.setColor(QPalette::Dark, QColor(0x12, 0x12, 0x12));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Highlight, QColor(0x4a, 0x4a, 0x4a));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, QColor(0x5b, 0x9b, 0xd5));
    p.setColor(QPalette::LinkVisited, QColor(0x8f, 0x8f, 0xd5));
    p.setColor(QPalette::ToolTipBase, QColor(0x2c, 0x2c, 0x2c));
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8a, 0x8a));
    for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text,
                                      QPalette::ButtonText })
        p.setColor(QPalette::Disabled, role, dimmed);
    return p;
}

void WindowTheme::install(qt_intf_t *intf, QApplication &app, bool changeStyle)
{
    const bool dark = isDark(intf);
    /* The native Windows style draws dialogs light whatever the palette;
     * Fusion follows it */
    if (changeStyle && dark && QApplication::setStyle(QStringLiteral("Fusion")))
        QApplication::setPalette(darkPalette());
    app.installEventFilter(new WindowTheme(intf, &app));
}

bool WindowTheme::eventFilter(QObject *watched, QEvent *event)
{
#ifdef _WIN32
    if (event->type() == QEvent::Show)
    {
        auto widget = qobject_cast<QWidget *>(watched);
        if (widget && widget->isWindow())
        {
            using SetAttribute = HRESULT (WINAPI *)(HWND, DWORD, LPCVOID, DWORD);
            static const auto setAttribute = reinterpret_cast<SetAttribute>(
                QLibrary::resolve(QStringLiteral("dwmapi"), "DwmSetWindowAttribute"));
            if (setAttribute)
            {
                const BOOL dark = isDark(m_intf);
                const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
                /* DWMWA_USE_IMMERSIVE_DARK_MODE is 20, and 19 before
                 * Windows 10 20H1 */
                if (FAILED(setAttribute(hwnd, 20, &dark, sizeof(dark))))
                    setAttribute(hwnd, 19, &dark, sizeof(dark));
            }
        }
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return false;
}

}
