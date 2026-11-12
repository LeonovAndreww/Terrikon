/*****************************************************************************
 * diagnostics.hpp: what happened, for finding out why
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
#ifndef TERRIKON_DIAGNOSTICS_HPP
#define TERRIKON_DIAGNOSTICS_HPP

#include "qt.hpp"

#include <QObject>
#include <QString>

class QGuiApplication;

namespace terrikon {

/* A journal of each run, in %LOCALAPPDATA%\terrikon\logs (the last ten):
 * what was clicked and where, the pages, the media played and the player's
 * states, Qt's and QML's warnings, the errors shown. A watchdog notes when
 * the interface stops responding for more than a few seconds, with a dump
 * of the process (crashes folder) to see where it was stuck. */
class Diagnostics : public QObject
{
    Q_OBJECT
public:
    static Diagnostics *install(qt_intf_t *intf, QGuiApplication &app);
    static Diagnostics *instance();
    ~Diagnostics() override;

    /* a line in the journal, from anywhere (QML too) */
    Q_INVOKABLE void note(const QString &what);
    Q_INVOKABLE QString folder() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

    struct Private;

private:
    Diagnostics(qt_intf_t *intf, QObject *parent);
    Private *d;
};

}

#endif
