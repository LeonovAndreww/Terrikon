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
#ifndef TERRIKON_MEDIA_FOLDERS_HPP
#define TERRIKON_MEDIA_FOLDERS_HPP

#include <QObject>
#include <QUrl>
#include <QVariantList>

namespace terrikon {

/* Folders worth putting in the media library, found on this computer:
 * where game recorders save (NVIDIA, Xbox Game Bar, OBS) and the Videos
 * folder, so that they can be added with one click. */
class MediaFolders : public QObject
{
    Q_OBJECT

    /* What is written under the covers of Videos (see VideoFields) */
    Q_PROPERTY(QStringList videoFields READ videoFields NOTIFY videoFieldsChanged FINAL)

public:
    explicit MediaFolders(QObject *parent = nullptr);

    QStringList videoFields() const;

signals:
    void videoFieldsChanged();
    /* the answer to requestSuggestions() */
    void suggestionsReady(const QVariantList &suggestions);

public:

    /* [{ title, url, path, videos }], only folders that exist and hold
     * videos (music for the Music folder), and are not in have (urls of
     * the folders in the media library); videos is how many, counted two
     * levels deep, at most 999 */
    Q_INVOKABLE QVariantList suggestions(const QStringList &have = {}) const;

    /* The same, worked out on a thread of its own and sent with
     * suggestionsReady(): looking at folders can take a minute when one of
     * them is on a network drive that does not answer */
    Q_INVOKABLE void requestSuggestions(const QStringList &have);

    /* Asks for a folder; empty if cancelled */
    Q_INVOKABLE QUrl pickFolder() const;

    /* How many videos a folder holds, two levels deep, at most 999 */
    Q_INVOKABLE int countVideos(const QUrl &folder) const;

    /* Whether a dropped url is a folder on this computer */
    Q_INVOKABLE bool isFolder(const QUrl &url) const;

    /* A menu at the pointer for folders dropped on the window: 0 to play
     * them, 1 to add them to the media library, -1 if dismissed */
    Q_INVOKABLE int askDropAction() const;
};

}

#endif
