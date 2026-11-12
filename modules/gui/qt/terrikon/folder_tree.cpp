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

#include "folder_tree.hpp"
#include "qt.hpp"

#include "medialibrary/mlhelper.hpp"

#include <QMenu>
#include <QSettings>
#include <QStringList>

namespace terrikon {

static QSettings settings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("terrikon"), QStringLiteral("library"));
}

FolderExpansion::FolderExpansion()
{
    const QStringList list = settings().value(QStringLiteral("Videos/expanded-folders")).toStringList();
    m_expanded = QSet<QString>(list.begin(), list.end());
}

FolderExpansion *FolderExpansion::instance()
{
    static FolderExpansion *self = new FolderExpansion;
    return self;
}

QSet<QString> FolderExpansion::expanded() const
{
    QMutexLocker locker(&m_lock);
    return m_expanded;
}

void FolderExpansion::save() const
{
    const QSet<QString> set = expanded();
    settings().setValue(QStringLiteral("Videos/expanded-folders"), QStringList(set.begin(), set.end()));
}

void FolderExpansion::expand(const QString &mrl)
{
    {
        QMutexLocker locker(&m_lock);
        if (mrl.isEmpty() || m_expanded.contains(mrl))
            return;
        m_expanded.insert(mrl);
    }
    save();
    emit changed();
}

void FolderExpansion::collapse(const QString &mrl)
{
    bool any = false;
    {
        QMutexLocker locker(&m_lock);
        for (auto it = m_expanded.begin(); it != m_expanded.end();)
        {
            if (!mrl.isEmpty() && it->startsWith(mrl))
            {
                it = m_expanded.erase(it);
                any = true;
            }
            else
                ++it;
        }
    }
    if (!any)
        return;
    save();
    emit changed();
}

VideoFields::VideoFields()
{
    m_fields = settings().value(QStringLiteral("Videos/fields"),
                                QStringList{ QStringLiteral("duration"), QStringLiteral("size") }).toStringList();
}

VideoFields *VideoFields::instance()
{
    static VideoFields *self = new VideoFields;
    return self;
}

QStringList VideoFields::all()
{
    return { QStringLiteral("duration"), QStringLiteral("count"), QStringLiteral("size"),
             QStringLiteral("quality"), QStringLiteral("folder"),
             QStringLiteral("mtime"), QStringLiteral("last_played") };
}

void VideoFields::fillMenu(QMenu *menu)
{
    const struct { const char *title; const char *field; } shown[] = {
        { N_("Length"), "duration" },
        { N_("Number of videos"), "count" },
        { N_("Size"), "size" },
        { N_("Quality (HD, 4K...)"), "quality" },
        { N_("Folder"), "folder" },
        { N_("Date modified"), "mtime" },
        { N_("Last watched"), "last_played" },
    };
    for (const auto &entry : shown)
    {
        QAction *action = menu->addAction(qtr(entry.title));
        action->setCheckable(true);
        const QString field = QString::fromLatin1(entry.field);
        action->setChecked(instance()->fields().contains(field));
        QObject::connect(action, &QAction::toggled, instance(), [field](bool on) {
            instance()->setShown(field, on);
        });
    }
}

void VideoFields::setShown(const QString &field, bool shown)
{
    if (m_fields.contains(field) == shown)
        return;
    QStringList fields;
    for (const QString &f : all())
        if (f == field ? shown : m_fields.contains(f))
            fields << f;
    m_fields = fields;
    settings().setValue(QStringLiteral("Videos/fields"), m_fields);
    emit changed();
}

QString folderKey(const char *mrl)
{
    QString key = QString::fromUtf8(mrl ? mrl : "");
    if (!key.isEmpty() && !key.endsWith(QLatin1Char('/')))
        key += QLatin1Char('/');
    return key;
}

static bool visit(vlc_medialibrary_t *ml, int64_t folderId,
                  const std::function<bool(const vlc_ml_media_t &)> &fn, int depth)
{
    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    params.i_sort = VLC_ML_SORTING_ALPHA;

    ml_unique_ptr<vlc_ml_media_list_t> media(vlc_ml_list_media_of(ml, &params, VLC_ML_PARENT_FOLDER, folderId));
    if (media)
        for (const vlc_ml_media_t &m : ml_range_iterate<vlc_ml_media_t>(media))
            if (!fn(m))
                return false;

    if (depth > 32)
        return true;

    vlc_ml_folder_list_t *subfolders = nullptr;
    if (vlc_ml_list(ml, VLC_ML_LIST_SUBFOLDERS, &params, folderId, &subfolders) != VLC_SUCCESS || !subfolders)
        return true;
    ml_unique_ptr<vlc_ml_folder_list_t> owner(subfolders);
    for (const vlc_ml_folder_t &sub : ml_range_iterate<vlc_ml_folder_t>(owner))
        if (!sub.b_banned && !visit(ml, sub.i_id, fn, depth + 1))
            return false;
    return true;
}

void forEachFolderMedia(vlc_medialibrary_t *ml, int64_t folderId,
                        const std::function<bool(const vlc_ml_media_t &)> &fn)
{
    visit(ml, folderId, fn, 0);
}

}
