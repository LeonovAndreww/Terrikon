/*****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mlvideofoldersmodel.hpp"

// VLC includes
#include <vlc_media_library.h>

// Util includes
#include "util/vlctick.hpp"

// MediaLibrary includes
#include "mlcustomcover.hpp"
#include "mlhelper.hpp"
#include "mlvideo.hpp"

#include "terrikon/folder_tree.hpp"

#include <QCollator>
#include <QDateTime>
#include <QHash>
#include <QLocale>
#include <QUrl>

#include <algorithm>

// NOTE: We multiply by 3 to cover most dpi settings.
static const int MLVIDEOFOLDERSMODEL_COVER_WIDTH  = 260 * 3; // 16 / 10 ratio
static const int MLVIDEOFOLDERSMODEL_COVER_HEIGHT = 162 * 3;

namespace {

/* What the folders and videos shown here know besides their own data */
struct Place
{
    QString collapseKey, collapseName;       // the folder collapsing goes back to
    QString collapseAllKey, collapseAllName; // the topmost one, as when added
    QString expandPreview;                   // what expanding shows
    int64_t size = 0;
    time_t mtime = 0;
    time_t lastPlayed = 0;
    QString path;
};

class TreeFolder : public MLItem
{
public:
    TreeFolder(int64_t id, const QString &key) : MLItem(MLItemId(id, VLC_ML_PARENT_FOLDER)), key(key) {}

    QString key;
    QString title;
    unsigned count = 0;
    int64_t durationMs = 0;
    Place place;
};

class TreeVideo : public MLVideo
{
public:
    explicit TreeVideo(const vlc_ml_media_t *media) : MLVideo(media) {}

    Place place;
};

struct Node
{
    int64_t id = 0;
    QString name, key;
    int parent = -1;
    bool entry = false;
    std::vector<int> children;
    std::vector<int> videos; // indexes in Tree::media

    unsigned count = 0;
    int64_t durationMs = 0, size = 0;
    time_t mtime = 0, lastPlayed = 0;
};

struct Tree
{
    std::vector<Node> nodes;
    QHash<QString, int> byKey; // lower case
    std::vector<int> roots;
    ml_unique_ptr<vlc_ml_media_list_t> media;
    std::vector<int64_t> mediaSize;
    std::vector<time_t> mediaMtime;
};

static QString parentKeyOf(const QString &key)
{
    const int slash = key.lastIndexOf(QLatin1Char('/'), key.size() - 2);
    if (slash < 0 || key.mid(slash - 1, 2) == QLatin1String(":/") || key.mid(slash - 2, 3) == QLatin1String("://"))
        return {};
    return key.left(slash + 1);
}

static void sumUp(Tree &tree, int i)
{
    Node &n = tree.nodes[i];
    for (int v : n.videos)
    {
        const vlc_ml_media_t &m = tree.media->p_items[v];
        n.count++;
        n.durationMs += std::max<int64_t>(0, m.i_duration);
        n.size += tree.mediaSize[v];
        n.mtime = std::max(n.mtime, tree.mediaMtime[v]);
        n.lastPlayed = std::max(n.lastPlayed, m.i_last_played_date);
    }
    for (int c : n.children)
    {
        sumUp(tree, c);
        const Node &child = tree.nodes[c];
        n.count += child.count;
        n.durationMs += child.durationMs;
        n.size += child.size;
        n.mtime = std::max(n.mtime, child.mtime);
        n.lastPlayed = std::max(n.lastPlayed, child.lastPlayed);
    }
}

static std::unique_ptr<Tree> buildTree(vlc_medialibrary_t *ml)
{
    auto tree = std::make_unique<Tree>();

    ml_unique_ptr<vlc_ml_folder_list_t> folders(vlc_ml_list_folders(ml, nullptr));
    ml_unique_ptr<vlc_ml_folder_list_t> entries(vlc_ml_list_entry_points(ml, nullptr));

    auto addFolder = [&](const vlc_ml_folder_t &f) {
        if (f.b_banned || !f.psz_mrl)
            return;
        const QString key = terrikon::folderKey(f.psz_mrl);
        if (tree->byKey.contains(key.toLower()))
            return;
        Node n;
        n.id = f.i_id;
        n.key = key;
        n.name = qfu(f.psz_name);
        if (n.name.isEmpty())
            n.name = QUrl::fromPercentEncoding(key.section(QLatin1Char('/'), -2, -2).toUtf8());
        tree->byKey.insert(key.toLower(), int(tree->nodes.size()));
        tree->nodes.push_back(std::move(n));
    };
    if (folders)
        for (const vlc_ml_folder_t &f : ml_range_iterate<vlc_ml_folder_t>(folders))
            addFolder(f);
    if (entries)
        for (const vlc_ml_folder_t &f : ml_range_iterate<vlc_ml_folder_t>(entries))
        {
            addFolder(f);
            if (f.psz_mrl)
            {
                const int i = tree->byKey.value(terrikon::folderKey(f.psz_mrl).toLower(), -1);
                if (i >= 0)
                    tree->nodes[i].entry = true;
            }
        }

    for (int i = 0; i < int(tree->nodes.size()); ++i)
    {
        Node &n = tree->nodes[i];
        if (n.entry)
        {
            tree->roots.push_back(i);
            continue;
        }
        for (QString up = parentKeyOf(n.key); !up.isEmpty(); up = parentKeyOf(up))
        {
            const int p = tree->byKey.value(up.toLower(), -1);
            if (p >= 0)
            {
                n.parent = p;
                tree->nodes[p].children.push_back(i);
                break;
            }
        }
        if (n.parent < 0)
            tree->roots.push_back(i);
    }

    vlc_ml_query_params_t params = vlc_ml_query_params_create();
    tree->media.reset(vlc_ml_list_video_media(ml, &params));
    const size_t mediaCount = tree->media ? tree->media->i_nb_items : 0;
    tree->mediaSize.assign(mediaCount, 0);
    tree->mediaMtime.assign(mediaCount, 0);
    for (size_t v = 0; v < mediaCount; ++v)
    {
        const vlc_ml_media_t &m = tree->media->p_items[v];
        if (!m.p_files)
            continue;
        for (size_t f = 0; f < m.p_files->i_nb_items; ++f)
        {
            const vlc_ml_file_t &file = m.p_files->p_items[f];
            if (file.i_type != VLC_ML_FILE_TYPE_MAIN || !file.psz_mrl)
                continue;
            const QString mrl = qfu(file.psz_mrl);
            const int node = tree->byKey.value(mrl.left(mrl.lastIndexOf(QLatin1Char('/')) + 1).toLower(), -1);
            if (node >= 0)
                tree->nodes[node].videos.push_back(int(v));
            tree->mediaSize[v] = file.i_size;
            tree->mediaMtime[v] = file.i_last_modification_date;
            break;
        }
    }

    for (int r : tree->roots)
        sumUp(*tree, r);
    return tree;
}

/* One thing shown: a folder or a video */
struct Shown
{
    bool video = false;
    int index = -1;       // in Tree::nodes or Tree::media
    int collapseTo = -1;  // node
    int collapseAllTo = -1;
};

static void place(const Tree &tree, int node, int producer, int top,
                  const QSet<QString> &expanded, std::vector<Shown> &out)
{
    const Node &n = tree.nodes[node];
    if (n.count == 0)
        return;
    if (expanded.contains(n.key))
    {
        const int t = (top < 0) ? node : top;
        for (int c : n.children)
            place(tree, c, node, t, expanded, out);
        for (int v : n.videos)
            out.push_back({ true, v, node, t });
    }
    else
        out.push_back({ false, node, producer, top });
}

static QString titleOf(const Tree &tree, const Shown &s)
{
    if (!s.video)
        return tree.nodes[s.index].name;
    const vlc_ml_media_t &m = tree.media->p_items[s.index];
    return qfu(m.psz_title);
}

static std::vector<Shown> shownItems(const Tree &tree, const MLItemId &parent,
                                     const QSet<QString> &expanded, const QString &pattern)
{
    std::vector<Shown> out;

    std::vector<int> scope;
    int parentNode = -1;
    if (parent.id != 0 && parent.type == VLC_ML_PARENT_FOLDER)
    {
        for (int i = 0; i < int(tree.nodes.size()); ++i)
            if (tree.nodes[i].id == parent.id)
                parentNode = i;
        if (parentNode < 0)
            return out;
        scope = tree.nodes[parentNode].children;
    }
    else
        scope = tree.roots;

    if (!pattern.isEmpty())
    {
        // Searching: the videos found anywhere inside, flat
        std::vector<int> stack = scope;
        std::vector<int> videos;
        if (parentNode >= 0)
            videos = tree.nodes[parentNode].videos;
        while (!stack.empty())
        {
            const Node &n = tree.nodes[stack.back()];
            stack.pop_back();
            videos.insert(videos.end(), n.videos.begin(), n.videos.end());
            stack.insert(stack.end(), n.children.begin(), n.children.end());
        }
        for (int v : videos)
        {
            const Shown s{ true, v, -1, -1 };
            if (titleOf(tree, s).contains(pattern, Qt::CaseInsensitive))
                out.push_back(s);
        }
        return out;
    }

    for (int i : scope)
        place(tree, i, -1, -1, expanded, out);
    if (parentNode >= 0)
        for (int v : tree.nodes[parentNode].videos)
            out.push_back({ true, v, -1, -1 });
    return out;
}

static void sortShown(const Tree &tree, std::vector<Shown> &items,
                      vlc_ml_sorting_criteria_t sort, bool desc)
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    struct Key { QString title; int64_t count, duration, size, mtime, last; bool video; };
    std::vector<Key> keys;
    keys.reserve(items.size());
    for (const Shown &s : items)
    {
        if (s.video)
        {
            const vlc_ml_media_t &m = tree.media->p_items[s.index];
            keys.push_back({ qfu(m.psz_title), 1, m.i_duration, tree.mediaSize[s.index],
                             tree.mediaMtime[s.index], m.i_last_played_date, true });
        }
        else
        {
            const Node &n = tree.nodes[s.index];
            keys.push_back({ n.name, n.count, n.durationMs, n.size, n.mtime, n.lastPlayed, false });
        }
    }

    std::vector<size_t> order(items.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;

    auto byTitle = [&](size_t a, size_t b) { return collator.compare(keys[a].title, keys[b].title) < 0; };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Key &ka = keys[a], &kb = keys[b];
        int64_t va = 0, vb = 0;
        switch (sort)
        {
            case VLC_ML_SORTING_ALPHA:
                return desc ? byTitle(b, a) : byTitle(a, b);
            case VLC_ML_SORTING_NB_VIDEO: va = ka.count; vb = kb.count; break;
            case VLC_ML_SORTING_DURATION: va = ka.duration; vb = kb.duration; break;
            case VLC_ML_SORTING_FILESIZE: va = ka.size; vb = kb.size; break;
            case VLC_ML_SORTING_LASTMODIFICATIONDATE: va = ka.mtime; vb = kb.mtime; break;
            case VLC_ML_SORTING_LASTPLAYBACKDATE: va = ka.last; vb = kb.last; break;
            default:
                // Folders first, then videos, each by name
                if (ka.video != kb.video)
                    return !ka.video;
                return byTitle(a, b);
        }
        if (va != vb)
            return desc ? va > vb : va < vb;
        return byTitle(a, b);
    });

    std::vector<Shown> sorted;
    sorted.reserve(items.size());
    for (size_t i : order)
        sorted.push_back(items[i]);
    items.swap(sorted);
}

static QString previewOf(const Tree &tree, int node)
{
    const Node &n = tree.nodes[node];
    QStringList names;
    for (int c : n.children)
        if (tree.nodes[c].count > 0)
            names << tree.nodes[c].name;
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
    const int shown = 8;
    QString text = QStringList(names.mid(0, shown)).join(QStringLiteral(", "));
    if (names.size() > shown)
        text += QLatin1Char(' ') + qtr("and %1 more").arg(names.size() - shown);
    if (!n.videos.empty())
    {
        if (!text.isEmpty())
            text += QStringLiteral(", ");
        text += qtr("%1 videos").arg(n.videos.size());
    }
    return text;
}

static std::unique_ptr<MLItem> makeItem(const Tree &tree, const Shown &s)
{
    Place p;
    if (s.collapseTo >= 0)
    {
        p.collapseKey = tree.nodes[s.collapseTo].key;
        p.collapseName = tree.nodes[s.collapseTo].name;
    }
    if (s.collapseAllTo >= 0 && s.collapseAllTo != s.collapseTo)
    {
        p.collapseAllKey = tree.nodes[s.collapseAllTo].key;
        p.collapseAllName = tree.nodes[s.collapseAllTo].name;
    }

    if (s.video)
    {
        const vlc_ml_media_t &m = tree.media->p_items[s.index];
        auto item = std::make_unique<TreeVideo>(&m);
        p.size = tree.mediaSize[s.index];
        p.mtime = tree.mediaMtime[s.index];
        p.lastPlayed = m.i_last_played_date;
        item->place = p;
        return item;
    }

    const Node &n = tree.nodes[s.index];
    auto item = std::make_unique<TreeFolder>(n.id, n.key);
    item->title = n.name;
    item->count = n.count;
    item->durationMs = n.durationMs;
    p.size = n.size;
    p.mtime = n.mtime;
    p.lastPlayed = n.lastPlayed;
    p.expandPreview = previewOf(tree, s.index);
    p.path = QUrl(n.key).toLocalFile();
    item->place = p;
    return item;
}

static const Place *placeOf(const MLItem *item)
{
    if (!item)
        return nullptr;
    if (item->getId().type == VLC_ML_PARENT_FOLDER)
        return &static_cast<const TreeFolder *>(item)->place;
    if (auto video = dynamic_cast<const TreeVideo *>(item))
        return &video->place;
    return nullptr;
}

static QString sizeText(int64_t bytes)
{
    if (bytes <= 0)
        return {};
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

static QString dateText(time_t date)
{
    if (date <= 0)
        return {};
    return QLocale().toString(QDateTime::fromSecsSinceEpoch(date).date(), QLocale::ShortFormat);
}

}

// Ctor / dtor

/* explicit */ MLVideoFoldersModel::MLVideoFoldersModel(QObject * parent) : MLVideoModel(parent)
{
    connect(terrikon::FolderExpansion::instance(), &terrikon::FolderExpansion::changed,
            this, &MLVideoFoldersModel::resetRequested);
}

// MLBaseModel reimplementation

QHash<int, QByteArray> MLVideoFoldersModel::roleNames() const /* override */
{
    QHash<int, QByteArray> hash = MLVideoModel::roleNames();
    hash.insert({
        { FOLDER_COUNT, QByteArrayLiteral("count") },
        { FOLDER_NB_VIDEOS, QByteArrayLiteral("nb_videos") },
        { FOLDER_SIZE, QByteArrayLiteral("size") },
        { FOLDER_SIZE_TEXT, QByteArrayLiteral("size_text") },
        { FOLDER_MTIME_TEXT, QByteArrayLiteral("mtime_text") },
        { FOLDER_LAST_PLAYED_TEXT, QByteArrayLiteral("last_played_text") },
        { FOLDER_PATH, QByteArrayLiteral("path") },
        { FOLDER_CAN_EXPAND, QByteArrayLiteral("canExpand") },
        { FOLDER_CAN_COLLAPSE, QByteArrayLiteral("canCollapse") },
        { FOLDER_CAN_COLLAPSE_ALL, QByteArrayLiteral("canCollapseAll") },
    });
    return hash;
}

QVariant MLVideoFoldersModel::itemRoleData(const MLItem * item, const int role) const /* override */
{
    if (item == nullptr)
        return QVariant();

    const Place *p = placeOf(item);

    switch (role)
    {
        case FOLDER_SIZE:
            return p ? QVariant::fromValue<qint64>(p->size) : QVariant();
        case FOLDER_SIZE_TEXT:
            return p ? sizeText(p->size) : QString();
        case FOLDER_MTIME_TEXT:
            return p ? dateText(p->mtime) : QString();
        case FOLDER_LAST_PLAYED_TEXT:
            return p ? dateText(p->lastPlayed) : QString();
        case FOLDER_CAN_COLLAPSE:
            return p && !p->collapseKey.isEmpty();
        case FOLDER_CAN_COLLAPSE_ALL:
            return p && !p->collapseAllKey.isEmpty();
        default:
            break;
    }

    if (item->getId().type != VLC_ML_PARENT_FOLDER)
    {
        switch (role)
        {
            case FOLDER_COUNT:
            case FOLDER_NB_VIDEOS:
                return 1;
            case FOLDER_CAN_EXPAND:
                return false;
            case FOLDER_PATH:
                return QVariant();
            default:
                return MLVideoModel::itemRoleData(item, role);
        }
    }

    const auto folder = static_cast<const TreeFolder *>(item);
    switch (role)
    {
        // NOTE: This is the condition for QWidget view(s).
        case Qt::DisplayRole:
        case MEDIA_TITLE:
            return QVariant::fromValue(folder->title);
        case MEDIA_ID:
            return QVariant::fromValue(folder->getId());
        case MEDIA_TITLE_FIRST_SYMBOL:
            return QVariant::fromValue(getFirstSymbol(folder->title));
        case MEDIA_SMALL_COVER:
        case VIDEO_THUMBNAIL:
            return MLCustomCover::url(folder->getId()
                                    , QSize(MLVIDEOFOLDERSMODEL_COVER_WIDTH, MLVIDEOFOLDERSMODEL_COVER_HEIGHT)
                                    , QStringLiteral(":/placeholder/noart_videoCover.svg"));
        case MEDIA_DURATION:
            return QVariant::fromValue(VLCDuration::fromMS(folder->durationMs));
        case MEDIA_IS_VIDEO:
            return false;
        case MEDIA_PROGRESS:
            return 0.0;
        case FOLDER_COUNT:
        case FOLDER_NB_VIDEOS:
            return QVariant::fromValue(folder->count);
        case FOLDER_CAN_EXPAND:
            return folder->count > 0;
        case FOLDER_PATH:
            return folder->place.path;
        default:
            return QVariant();
    }
}

vlc_ml_sorting_criteria_t MLVideoFoldersModel::nameToCriteria(QByteArray name) const /* override */
{
    return QHash<QByteArray, vlc_ml_sorting_criteria_t> {
        { QByteArrayLiteral("title"),       VLC_ML_SORTING_ALPHA },
        { QByteArrayLiteral("duration"),    VLC_ML_SORTING_DURATION },
        { QByteArrayLiteral("nb_videos"),   VLC_ML_SORTING_NB_VIDEO },
        { QByteArrayLiteral("size"),        VLC_ML_SORTING_FILESIZE },
        { QByteArrayLiteral("size_text"),   VLC_ML_SORTING_FILESIZE },
        { QByteArrayLiteral("mtime"),       VLC_ML_SORTING_LASTMODIFICATIONDATE },
        { QByteArrayLiteral("mtime_text"),  VLC_ML_SORTING_LASTMODIFICATIONDATE },
        { QByteArrayLiteral("last_played"), VLC_ML_SORTING_LASTPLAYBACKDATE },
        { QByteArrayLiteral("last_played_text"), VLC_ML_SORTING_LASTPLAYBACKDATE },
    }.value(name, VLC_ML_SORTING_DEFAULT);
}

std::unique_ptr<MLListCacheLoader>
MLVideoFoldersModel::createMLLoader() const /* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLVideoFoldersModel::Loader>(*this));
}

// Expanding, collapsing

QString MLVideoFoldersModel::expandPreview(const QModelIndex &index) const
{
    const Place *p = placeOf(item(index.row()));
    return p ? p->expandPreview : QString();
}

QString MLVideoFoldersModel::collapseTarget(const QModelIndex &index) const
{
    const Place *p = placeOf(item(index.row()));
    return p ? p->collapseName : QString();
}

QString MLVideoFoldersModel::collapseAllTarget(const QModelIndex &index) const
{
    const Place *p = placeOf(item(index.row()));
    return p ? p->collapseAllName : QString();
}

void MLVideoFoldersModel::expandItem(const QModelIndex &index)
{
    const MLItem *it = item(index.row());
    if (it && it->getId().type == VLC_ML_PARENT_FOLDER)
        terrikon::FolderExpansion::instance()->expand(static_cast<const TreeFolder *>(it)->key);
}

void MLVideoFoldersModel::collapseItem(const QModelIndex &index)
{
    if (const Place *p = placeOf(item(index.row())))
        terrikon::FolderExpansion::instance()->collapse(p->collapseKey);
}

void MLVideoFoldersModel::collapseAllItem(const QModelIndex &index)
{
    if (const Place *p = placeOf(item(index.row())))
        terrikon::FolderExpansion::instance()->collapse(p->collapseAllKey.isEmpty() ? p->collapseKey
                                                                                    : p->collapseAllKey);
}

// Protected MLBaseModel reimplementation

void MLVideoFoldersModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_FOLDER_ADDED:
        case VLC_ML_EVENT_FOLDER_DELETED:
        case VLC_ML_EVENT_MEDIA_DELETED:
            emit resetRequested();
            return;
        case VLC_ML_EVENT_FOLDER_UPDATED:
            return;
        default:
            break;
    }

    MLVideoModel::onVlcMlEvent(event);
}

// Loader

MLVideoFoldersModel::Loader::Loader(const MLVideoFoldersModel &model)
    : MLOp(model)
{
}

size_t MLVideoFoldersModel::Loader::count(vlc_medialibrary_t * ml, const vlc_ml_query_params_t*) const /* override */
{
    const auto tree = buildTree(ml);
    return shownItems(*tree, m_parent, terrikon::FolderExpansion::instance()->expanded(), QString::fromUtf8(m_searchPattern)).size();
}

std::vector<std::unique_ptr<MLItem>>
MLVideoFoldersModel::Loader::load(vlc_medialibrary_t * ml,
                                  const vlc_ml_query_params_t* queryParams) const /* override */
{
    const auto tree = buildTree(ml);
    std::vector<Shown> items = shownItems(*tree, m_parent, terrikon::FolderExpansion::instance()->expanded(), QString::fromUtf8(m_searchPattern));
    sortShown(*tree, items, m_sort, m_sortDesc);

    const size_t offset = queryParams ? queryParams->i_offset : 0;
    const size_t limit = (queryParams && queryParams->i_nbResults) ? queryParams->i_nbResults : items.size();

    std::vector<std::unique_ptr<MLItem>> result;
    for (size_t i = offset; i < items.size() && i < offset + limit; ++i)
        result.emplace_back(makeItem(*tree, items[i]));
    return result;
}

std::unique_ptr<MLItem>
MLVideoFoldersModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    const auto tree = buildTree(ml);
    for (const Shown &s : shownItems(*tree, m_parent, terrikon::FolderExpansion::instance()->expanded(), QString::fromUtf8(m_searchPattern)))
    {
        const int64_t id = s.video ? tree->media->p_items[s.index].i_id : tree->nodes[s.index].id;
        const vlc_ml_parent_type type = s.video ? VLC_ML_PARENT_UNKNOWN : VLC_ML_PARENT_FOLDER;
        if (id == itemId.id && type == itemId.type)
            return makeItem(*tree, s);
    }
    return nullptr;
}
