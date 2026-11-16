/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
#include "networkbasemodel.hpp"

#include <QDir>
#include <QStandardPaths>
#ifdef _WIN32
# include <windows.h>
#endif

QVariant NetworkBaseModel::basedata(const NetworkBaseItem& item, int role) const
{
    switch (role)
    {
    case NETWORK_BASE_NAME:
        return item.name.trimmed();
    case NETWORK_BASE_MRL:
        return item.mainMrl;
    case NETWORK_BASE_TYPE:
        return item.type;
    case NETWORK_BASE_PROTOCOL:
        return item.protocol;
    case NETWORK_BASE_ARTWORK:
        return item.artwork;
    case NETWORK_BASE_ARTWORK_FALLBACK:
        if (item.type == TYPE_DIRECTORY || item.type == TYPE_NODE)
        {
            /* Terrikon: the places told apart, not one folder for all */
            const QString place = placeIcon(item.mainMrl);
            if (!place.isEmpty())
                return place;
        }
        return artworkFallbackForType(item.type);
    default:
        return {};
    };
}

/* Terrikon: what a place is, from its address: a folder of the user's
 * (videos, music...), a drive by its kind, a computer of the network */
QString NetworkBaseModel::placeIcon(const QUrl &url)
{
    const auto icon = [](const char *name) {
        return QStringLiteral("qrc:///sd/place_%1.svg").arg(QLatin1String(name));
    };
    if (url.scheme() == QLatin1String("smb") || url.scheme() == QLatin1String("nfs")
        || url.scheme() == QLatin1String("upnp"))
        return (url.path().isEmpty() || url.path() == QLatin1String("/")) ? icon("laptop") : QString();
    if (!url.isLocalFile())
        return {};
    const QString path = QDir::cleanPath(url.toLocalFile());
    static const struct { QStandardPaths::StandardLocation where; const char *name; } folders[] = {
        { QStandardPaths::MoviesLocation, "video" },
        { QStandardPaths::DocumentsLocation, "document" },
        { QStandardPaths::DownloadLocation, "arrow_download" },
        { QStandardPaths::MusicLocation, "music_note_2" },
        { QStandardPaths::DesktopLocation, "desktop" },
    };
    for (const auto &f : folders)
        if (path.compare(QDir::cleanPath(QStandardPaths::writableLocation(f.where)), Qt::CaseInsensitive) == 0)
            return icon(f.name);
#ifdef _WIN32
    /* a drive's root: "C:/" */
    if (path.size() <= 3 && path.size() >= 2 && path[1] == QLatin1Char(':'))
    {
        const std::wstring root = QString(path.left(2) + QLatin1Char('\\')).toStdWString();
        switch (GetDriveTypeW(root.c_str()))
        {
        case DRIVE_CDROM:
            return QStringLiteral("qrc:///sd/disc.svg");
        case DRIVE_REMOVABLE:
            return icon("usb_stick");
        case DRIVE_REMOTE:
            return icon("server");
        default:
            return icon("hard_drive");
        }
    }
#endif
    return {};
}

QString NetworkBaseModel::artworkFallbackForType(const ItemType type) const
{
    switch (type) {
    case TYPE_DISC:
        return QStringLiteral("qrc:///sd/disc.svg");
    case TYPE_CARD:
        return QStringLiteral("qrc:///sd/capture-card.svg");
    case TYPE_STREAM:
        return QStringLiteral("qrc:///sd/stream.svg");
    case TYPE_PLAYLIST:
        return QStringLiteral("qrc:///sd/playlist.svg");
    case TYPE_FILE:
        return QStringLiteral("qrc:///sd/file.svg");
    default:
        return QStringLiteral("qrc:///sd/directory.svg");
    }
}

QHash<int, QByteArray> NetworkBaseModel::roleNames() const
{
    return {
        { NETWORK_BASE_NAME, QByteArrayLiteral("name") },
        { NETWORK_BASE_MRL, QByteArrayLiteral("mrl") },
        { NETWORK_BASE_TYPE, QByteArrayLiteral("type") },
        { NETWORK_BASE_PROTOCOL, QByteArrayLiteral("protocol") },
        { NETWORK_BASE_ARTWORK, QByteArrayLiteral("artwork") },
        { NETWORK_BASE_ARTWORK_FALLBACK, QByteArrayLiteral("artworkFallback") },
    };
}
