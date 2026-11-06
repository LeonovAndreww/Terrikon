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

#include "mlrecentvideomodel.hpp"
#include "mlhelper.hpp"

#include <QUrl>

// Ctor / dtor

MLRecentVideoModel::MLRecentVideoModel(QObject * parent) : MLVideoModel(parent) {}

// Protected MLBaseModel implementation

std::unique_ptr<MLListCacheLoader>
MLRecentVideoModel::createMLLoader() const
/* override */
{
    return std::make_unique<MLListCacheLoader>(m_mediaLib, std::make_shared<MLRecentVideoModel::Loader>(*this));
}

// Private MLVideoModel reimplementation

void MLRecentVideoModel::onVlcMlEvent(const MLEvent & event) /* override */
{
    switch (event.i_type)
    {
        case VLC_ML_EVENT_HISTORY_CHANGED:
            emit resetRequested();
            return;
        default:
            break;
    }

    MLVideoModel::onVlcMlEvent(event);
}

// Loader

/* Terrikon: a video opened from a phone or from an archive is played from
 * a copy Windows makes in a temporary folder; going on with it later makes
 * no sense, and its folder is not one to show. Such copies stay out. */
static bool isTemporaryCopy(const vlc_ml_media_t &media)
{
    if (media.p_files == nullptr)
        return false;
    for (size_t i = 0; i < media.p_files->i_nb_items; ++i)
    {
        const vlc_ml_file_t &file = media.p_files->p_items[i];
        if (file.i_type != VLC_ML_FILE_TYPE_MAIN || file.psz_mrl == nullptr)
            continue;
        const QString mrl = QUrl::fromPercentEncoding(QByteArray(file.psz_mrl)).toLower();
        return mrl.contains(QLatin1String("/appdata/local/microsoft/windows/inetcache/"))
            || mrl.contains(QLatin1String("/appdata/local/temp/"))
            || mrl.contains(QLatin1String("/windows/temp/"));
    }
    return false;
}

/* The history without the temporary copies; offset and count apply after */
static std::vector<std::unique_ptr<MLItem>>
listKept(vlc_medialibrary_t *ml, const vlc_ml_query_params_t *queryParams, size_t *total)
{
    vlc_ml_query_params_t all = queryParams ? *queryParams : vlc_ml_query_params_create();
    all.i_offset = 0;
    all.i_nbResults = 0;
    ml_unique_ptr<vlc_ml_media_list_t> media_list { vlc_ml_list_video_history(ml, &all) };

    std::vector<std::unique_ptr<MLItem>> res;
    size_t kept = 0;
    const size_t offset = queryParams ? queryParams->i_offset : 0;
    const size_t limit = (queryParams && queryParams->i_nbResults) ? queryParams->i_nbResults : SIZE_MAX;
    if (media_list)
        for (const vlc_ml_media_t &media : ml_range_iterate<vlc_ml_media_t>(media_list))
        {
            if (isTemporaryCopy(media))
                continue;
            if (kept >= offset && res.size() < limit && !total)
                res.emplace_back(std::make_unique<MLVideo>(&media));
            kept++;
        }
    if (total)
        *total = kept;
    return res;
}

size_t MLRecentVideoModel::Loader::count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    size_t total = 0;
    listKept(ml, queryParams, &total);
    return total;
}

std::vector<std::unique_ptr<MLItem>>
MLRecentVideoModel::Loader::load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const /* override */
{
    return listKept(ml, queryParams, nullptr);
}

std::unique_ptr<MLItem>
MLRecentVideoModel::Loader::loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const
{
    assert(itemId.type == VLC_ML_PARENT_UNKNOWN);
    ml_unique_ptr<vlc_ml_media_t> media(vlc_ml_get_media(ml, itemId.id));
    if (!media)
        return nullptr;
    return std::make_unique<MLVideo>(media.get());
}
