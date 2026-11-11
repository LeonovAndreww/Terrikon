/*****************************************************************************
 * moment_marks.cpp: marked moments of the playing file
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

#include "moment_marks.hpp"
#include "media_tools.hpp"

#include <vlc_common.h>
#include <vlc_configuration.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace terrikon {

/* One small JSON file per media, named after its path and size: replays
 * are never edited in place, but file names do get reused */
static QString marksFileFor(const QString &mediaPath)
{
    const QFileInfo info(mediaPath);
    const QByteArray key = info.absoluteFilePath().toUtf8() + '|'
                         + QByteArray::number(info.size());
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16));

    char *base = config_GetUserDir(VLC_USERDATA_DIR);
    const QString root = base ? QString::fromUtf8(base) : QDir::homePath();
    free(base);

    return QDir(root).filePath(QStringLiteral("marks/%1.json").arg(hash));
}

void MomentMarks::loadUrl(const QUrl &url)
{
    /* the file name needs the media's size: looked up away from the
     * interface thread (see offThread) */
    const QString path = url.isLocalFile() ? url.toLocalFile() : QString();
    m_wanted = path;
    if (path.isEmpty())
    {
        loadFile({});
        return;
    }
    offThread(this, [path] { return marksFileFor(path); }, [this, path](const QString &file) {
        if (path == m_wanted)
            loadFile(file);
    });
}

void MomentMarks::loadFile(const QString &file)
{
    if (file == m_file)
        return;

    m_file = file;
    m_times.clear();

    QFile in(m_file);
    if (!m_file.isEmpty() && in.open(QIODevice::ReadOnly))
    {
        const QJsonArray array = QJsonDocument::fromJson(in.readAll()).array();
        for (const QJsonValue &value : array)
            if (value.isDouble() && value.toDouble() >= 0.)
                m_times.append(value.toDouble());
        std::sort(m_times.begin(), m_times.end());
    }
    emit changed();
}

void MomentMarks::save() const
{
    if (m_file.isEmpty())
        return;

    if (m_times.isEmpty())
    {
        QFile::remove(m_file);
        return;
    }

    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QJsonArray array;
    for (double time : m_times)
        array.append(time);

    /* Written aside and renamed, so a crash never leaves half a file */
    QSaveFile out(m_file);
    if (out.open(QIODevice::WriteOnly))
    {
        out.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
        out.commit();
    }
}

void MomentMarks::toggle(double seconds)
{
    if (m_file.isEmpty() || seconds < 0.)
        return;

    auto near = std::find_if(m_times.begin(), m_times.end(), [seconds](double time) {
        return std::abs(time - seconds) < sameMoment;
    });
    if (near != m_times.end())
        m_times.erase(near);
    else
        m_times.insert(std::lower_bound(m_times.begin(), m_times.end(), seconds), seconds);

    save();
    emit changed();
}

void MomentMarks::removeAt(int index)
{
    if (index < 0 || index >= m_times.size())
        return;
    m_times.remove(index);
    save();
    emit changed();
}

void MomentMarks::clearAll()
{
    if (m_times.isEmpty())
        return;
    m_times.clear();
    save();
    emit changed();
}

double MomentMarks::snap(double seconds, double tolerance) const
{
    double best = seconds;
    double bestDistance = tolerance;
    for (double time : m_times)
    {
        const double distance = std::abs(time - seconds);
        if (distance <= bestDistance)
        {
            best = time;
            bestDistance = distance;
        }
    }
    return best;
}

double MomentMarks::next(double seconds) const
{
    /* The half second keeps a jump from landing on the mark just reached */
    auto it = std::upper_bound(m_times.cbegin(), m_times.cend(), seconds + .5);
    return it == m_times.cend() ? -1. : *it;
}

double MomentMarks::previous(double seconds) const
{
    auto it = std::lower_bound(m_times.cbegin(), m_times.cend(), seconds - backGrace);
    return it == m_times.cbegin() ? -1. : *(it - 1);
}

QVariantList MomentMarks::times() const
{
    QVariantList list;
    list.reserve(m_times.size());
    for (double time : m_times)
        list.append(time);
    return list;
}

}
