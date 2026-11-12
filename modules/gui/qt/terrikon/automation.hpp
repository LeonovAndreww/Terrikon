/*****************************************************************************
 * automation.hpp: driving the interface from scripts
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
#ifndef TERRIKON_AUTOMATION_HPP
#define TERRIKON_AUTOMATION_HPP

#include "qt.hpp"

#include <QObject>

class QGuiApplication;

namespace terrikon {

/* Test scripts and assistants drive the interface through this rather than
 * through screenshots and a borrowed mouse: on 127.0.0.1, one command per
 * line, one line of JSON back. Only with TERRIKON_AUTOMATION=<port> in the
 * environment; see test/terrikon/README.md for the commands. */
class Automation : public QObject
{
    Q_OBJECT
public:
    static void install(qt_intf_t *intf, QGuiApplication &app);
    ~Automation() override;
    struct Private;

private:
    Automation(qt_intf_t *intf, QObject *parent);
    Private *d;
};

}

#endif
