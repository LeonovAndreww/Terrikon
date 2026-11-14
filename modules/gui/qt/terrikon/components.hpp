/*****************************************************************************
 * components.hpp: the outside tools Terrikon uses (ffmpeg, yt-dlp)
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
#ifndef TERRIKON_COMPONENTS_HPP
#define TERRIKON_COMPONENTS_HPP

#include "qt.hpp"

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>

class QDialog;

namespace terrikon {

/* ffmpeg (with ffprobe: clips, waveforms, thumbnails) and yt-dlp (links
 * to video sites) are not part of Terrikon: this fetches them into
 * %LOCALAPPDATA%\terrikon\tools (no administrator rights needed), updates
 * yt-dlp by itself once a day (sites change, old versions stop working),
 * and offers them when a feature finds one missing. */
class Components : public QObject
{
    Q_OBJECT
public:
    explicit Components(qt_intf_t *intf, QObject *parent = nullptr);
    static Components *instance();

    /* "ffmpeg" or "yt-dlp" */
    Q_INVOKABLE bool installed(const QString &id) const;
    Q_INVOKABLE QString version(const QString &id) const;
    Q_INVOKABLE bool busy(const QString &id) const { return m_busy.contains(id); }
    /* While downloading: 0..1, -1 while the size is not known */
    double progress(const QString &id) const { return m_progress.value(id, -1.); }
    QString progressText(const QString &id) const { return m_progressText.value(id); }
    /* Removes a component Terrikon downloaded */
    Q_INVOKABLE void remove(const QString &id);
    /* Whether it is the copy Terrikon downloaded (else one found in PATH) */
    bool ownCopy(const QString &id) const;
    static bool autoUpdate();
    static void setAutoUpdate(bool on);
    /* Downloads (or replaces) a component */
    Q_INVOKABLE void install(const QString &id);
    /* Asks the user whether to download a missing component a feature
     * needs, then does; "why" says for what */
    Q_INVOKABLE void offer(const QString &id, const QString &why);
    /* The window listing the components */
    Q_INVOKABLE void showDialog();
    /* the installer's "Components" box: download them now, once */
    void installAskedByInstaller();

signals:
    void changed(const QString &id);
    void progressChanged(const QString &id);
    void failed(const QString &id, const QString &error);

private:
    void updateYtdlpInBackground();
    void finish(const QString &id, const QString &error);
    QString toolsDir() const;

    qt_intf_t *m_intf;
    QHash<QString, bool> m_busy;
    QHash<QString, double> m_progress;
    QHash<QString, QString> m_progressText;
    mutable QHash<QString, QString> m_versions;
    mutable QSet<QString> m_probing; /* versions being asked */
    QPointer<QDialog> m_dialog;
    QHash<QString, bool> m_offered; /* once per run */
    static Components *s_instance;
};

}

#endif
