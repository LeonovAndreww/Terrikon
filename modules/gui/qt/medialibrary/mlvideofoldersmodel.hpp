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

#ifndef MLVIDEOFOLDERSMODEL_HPP
#define MLVIDEOFOLDERSMODEL_HPP

// MediaLibrary includes
#include "mlvideomodel.hpp"

/* Terrikon: the videos of the media library by folder, as the folders are
 * on the disk. A folder added to the media library shows as one folder,
 * subfolders and all; expanded, it is replaced by what it holds (its
 * subfolders and its own videos), one level, as often as wanted; collapsed
 * back the same way (see terrikon::FolderExpansion). With a parent folder,
 * what that folder holds is listed. */
class MLVideoFoldersModel : public MLVideoModel
{
    Q_OBJECT

    /* Terrikon: the folder just opened, for a second: what comes in from it
     * shows with an animation */
    Q_PROPERTY(QString lastExpanded READ lastExpanded NOTIFY lastExpandedChanged FINAL)

public:
    enum Roles
    {
        FOLDER_COUNT = MLVideoModel::VIDEO_ROLES_COUNT,
        FOLDER_NB_VIDEOS,
        FOLDER_SIZE,
        FOLDER_SIZE_TEXT,
        FOLDER_MTIME_TEXT,
        FOLDER_LAST_PLAYED_TEXT,
        FOLDER_PATH,
        FOLDER_CAN_EXPAND,
        FOLDER_CAN_COLLAPSE,
        FOLDER_CAN_COLLAPSE_ALL,
        FOLDER_GROUP, /* the expanded folder an item comes from, empty if none */
    };

public:
    explicit MLVideoFoldersModel(QObject * parent = nullptr);

    /* What an item becomes, for the tooltips of the buttons: the names of
     * what expanding shows, the folder collapsing goes back to */
    Q_INVOKABLE QString expandPreview(const QModelIndex &index) const;
    Q_INVOKABLE QString collapseTarget(const QModelIndex &index) const;
    Q_INVOKABLE QString collapseAllTarget(const QModelIndex &index) const;

    Q_INVOKABLE void expandItem(const QModelIndex &index);
    QString lastExpanded() const { return m_lastExpanded; }
    /* Terrikon: the folder a video brought in by expanding is directly in
     * (its tint), empty for anything else: neighbours with the same one join
     * their tinted cards */
    Q_INVOKABLE QString tintKeyAt(int row) const;

signals:
    void lastExpandedChanged();

public:
    Q_INVOKABLE void collapseItem(const QModelIndex &index);
    Q_INVOKABLE void collapseAllItem(const QModelIndex &index);

public: // MLBaseModel reimplementation
    QHash<int, QByteArray> roleNames() const override;

protected:
    QVariant itemRoleData(const MLItem *item, int role = Qt::DisplayRole) const override;

    vlc_ml_sorting_criteria_t nameToCriteria(QByteArray name) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

protected: // MLBaseModel reimplementation
    void onVlcMlEvent(const MLEvent & event) override;

private:
    struct Loader : public MLListCacheLoader::MLOp
    {
        Loader(const MLVideoFoldersModel &model);

        size_t count(vlc_medialibrary_t * ml, const vlc_ml_query_params_t* queryParams) const override;

        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t * ml,
                                                  const vlc_ml_query_params_t* queryParams) const override;

        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
    QString m_lastExpanded;
};

#endif // MLVIDEOFOLDERSMODEL_HPP
