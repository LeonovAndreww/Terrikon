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

#include "media_folders.hpp"

#include "qt.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace terrikon {

MediaFolders::MediaFolders(QObject *parent)
    : QObject(parent)
{
}

static const QStringList &videoPatterns()
{
    static const QStringList patterns = {
        QStringLiteral("*.mp4"), QStringLiteral("*.mkv"), QStringLiteral("*.mov"),
        QStringLiteral("*.avi"), QStringLiteral("*.webm"), QStringLiteral("*.flv"),
        QStringLiteral("*.ts"), QStringLiteral("*.m4v"), QStringLiteral("*.wmv"),
    };
    return patterns;
}

static int countIn(const QString &path, int depth)
{
    QDir dir(path);
    int count = dir.entryList(videoPatterns(), QDir::Files).size();
    if (depth > 0)
        for (const QString &sub : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            count += countIn(dir.filePath(sub), depth - 1);
            if (count >= 999)
                break;
        }
    return std::min(count, 999);
}

int MediaFolders::countVideos(const QUrl &folder) const
{
    return folder.isLocalFile() ? countIn(folder.toLocalFile(), 2) : 0;
}

/* Where OBS records, from the settings of each of its profiles */
static QStringList obsFolders()
{
    QStringList out;
    const QString profiles = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                                 .filePath(QStringLiteral("obs-studio/basic/profiles"));
    for (const QString &profile : QDir(profiles).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        QSettings ini(QDir(profiles).filePath(profile + QStringLiteral("/basic.ini")), QSettings::IniFormat);
        for (const char *key : { "SimpleOutput/FilePath", "AdvOut/RecFilePath" })
        {
            const QString path = QDir::fromNativeSeparators(ini.value(QLatin1String(key)).toString());
            if (!path.isEmpty() && !out.contains(path, Qt::CaseInsensitive))
                out << path;
        }
    }
    return out;
}

QVariantList MediaFolders::suggestions() const
{
    const QString videos = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    QList<QPair<QString, QString>> candidates = {
        { qtr("NVIDIA recordings"), QDir(videos).filePath(QStringLiteral("NVIDIA")) },
        { qtr("Xbox Game Bar recordings"), QDir(videos).filePath(QStringLiteral("Captures")) },
    };
    for (const QString &obs : obsFolders())
        candidates.append({ qtr("OBS recordings"), obs });
    candidates.append({ qtr("Videos"), videos });

    QVariantList out;
    QStringList seen;
    for (const auto &[title, path] : candidates)
    {
        const QFileInfo info(path);
        if (path.isEmpty() || !info.isDir())
            continue;
        const QString canonical = info.canonicalFilePath();
        if (seen.contains(canonical, Qt::CaseInsensitive))
            continue;
        seen << canonical;
        const int count = countIn(canonical, 2);
        if (count == 0)
            continue;
        out << QVariantMap{
            { QStringLiteral("title"), title },
            { QStringLiteral("url"), QUrl::fromLocalFile(canonical) },
            { QStringLiteral("path"), QDir::toNativeSeparators(canonical) },
            { QStringLiteral("videos"), count },
        };
    }
    return out;
}

QUrl MediaFolders::pickFolder() const
{
    const QString path = QFileDialog::getExistingDirectory(
        nullptr, qtr("Choose a folder with videos"),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
    return path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path);
}

}
