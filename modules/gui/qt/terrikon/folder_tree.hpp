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
#ifndef TERRIKON_FOLDER_TREE_HPP
#define TERRIKON_FOLDER_TREE_HPP

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>

#include <vlc_media_library.h>

namespace terrikon {

/* Which folders of the media library are shown opened in Videos, in place
 * of the folder itself ("expanded"), by MRL. A folder added to the media
 * library shows as one folder until expanded. Kept in the settings. */
class FolderExpansion : public QObject
{
    Q_OBJECT

public:
    static FolderExpansion *instance();

    QSet<QString> expanded() const { return m_expanded; }

    void expand(const QString &mrl);
    /* Collapses the folder and every folder under it */
    void collapse(const QString &mrl);

signals:
    void changed();

private:
    FolderExpansion();
    void save() const;

    QSet<QString> m_expanded;
};

/* What is written under the covers of Videos, in this order: "duration",
 * "count", "size", "mtime", "last_played". Kept in the settings. */
class VideoFields : public QObject
{
    Q_OBJECT

public:
    static VideoFields *instance();

    QStringList fields() const { return m_fields; }
    void setShown(const QString &field, bool shown);

    static QStringList all();

signals:
    void changed();

private:
    VideoFields();

    QStringList m_fields;
};

/* A folder MRL with one slash at the end, so that "is under" is a prefix
 * test */
QString folderKey(const char *mrl);

/* Calls fn on the media of a folder, then on those of its subfolders (in
 * the order of their names), down to the bottom, until fn returns false.
 * To be called on the media library thread. */
void forEachFolderMedia(vlc_medialibrary_t *ml, int64_t folderId,
                        const std::function<bool(const vlc_ml_media_t &)> &fn);

}

#endif
