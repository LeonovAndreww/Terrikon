/*****************************************************************************
 * terrikon_actions.hpp: Terrikon hotkey actions
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
#ifndef TERRIKON_TERRIKON_ACTIONS_HPP
#define TERRIKON_TERRIKON_ACTIONS_HPP

#include "qt.hpp"

#include <QObject>
#include <QString>

namespace terrikon {

class ClipSelection;
class CompactPlayer;
class MomentMarks;

/* Runs the Terrikon actions (clip, marks, picture in picture) when their
 * keys are pressed. The keys are VLC hotkeys like any other, set in
 * Preferences > Hotkeys; the core turns a key into an action and this
 * object, listening to "key-action", carries it out whatever view is
 * shown. */
class TerrikonActions : public QObject
{
    Q_OBJECT

public:
    TerrikonActions(qt_intf_t *intf, ClipSelection *clip, MomentMarks *marks,
                    CompactPlayer *compact, QObject *parent = nullptr);
    ~TerrikonActions() override;

    /* The key currently bound to a Terrikon action ("clip-toggle"...),
     * as written in the preferences, empty if none */
    Q_INVOKABLE QString keyFor(const QString &action) const;

signals:
    /* The clip panel owns the export settings, so it does the export */
    void clipExportRequested();

private:
    /* Called on the thread that pressed the key */
    static int keyActionCallback(vlc_object_t *, const char *, vlc_value_t,
                                 vlc_value_t newval, void *data);
    void run(int action);
    double nowSeconds() const;
    double lengthSeconds() const;

    qt_intf_t *m_intf;
    ClipSelection *m_clip;
    MomentMarks *m_marks;
    CompactPlayer *m_compact;
};

}

#endif
