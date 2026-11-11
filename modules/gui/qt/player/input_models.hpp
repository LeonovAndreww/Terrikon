/*****************************************************************************
 * input_models.hpp
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef INPUT_MODELS_HPP
#define INPUT_MODELS_HPP

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <memory>
#include <vlc_player.h>
#include <vlc_cxx_helpers.hpp>
#include <QAbstractListModel>
#include <QList>

/**
 * @brief The TrackListModel class represent the
 * (audio/video/spu/...)tracks of an input
 *
 * the model expose the track title using the Qt::DisplayRole
 * and the current track using Qt::CheckStateRole
 * we can select a track by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class QSettings;

class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)
    Q_PROPERTY(bool multiSelect READ getMultiSelect WRITE setMultiSelect NOTIFY multiSelectChanged FINAL)

public:
    enum TrackListRoles {
        /* per-track volume of audio tracks, 1.0 = unchanged */
        VolumeRole = Qt::UserRole + 1,
    };

    TrackListModel(vlc_player_t* player, QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &) const  override;

    int rowCount(const QModelIndex & = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void updateTracks(enum vlc_player_list_action action, const vlc_player_track *track_info);

    void updateTrackSelection(vlc_es_id_t *trackid, bool selected);

    void clear();

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_data.size(); }

    inline bool getMultiSelect() const { return m_multiSelect; }
    /* Selects an audio track in the background, silent, so that it can be
     * switched on instantly later. The player lock must be held. */
    void selectMuted(int row);
    /* The same for every audio track not playing, once one is */
    void addSilentTracks();

    /* [{ index, title, enabled, volume }] of every track, in order */
    Q_INVOKABLE QVariantList trackStates() const;

    /* Terrikon, audio: keeps the "several tracks" mode and the names of
     * the tracks switched on in "settings" under "key", and switches the
     * tracks of the same names on in the next media */
    void remember(QSettings *settings, const QString &key);

public slots:
    void setMultiSelect(bool multiSelect);

signals:
    void countChanged();
    void multiSelectChanged(bool multiSelect);

private:
    vlc_player_t* m_player;
    class Data {
    public:
        Data(const vlc_player_track *track_info);

        Data(const Data& data) = default;
        Data& operator =(const Data& data) = default;

        void update( const vlc_player_track *track_info );

        QString m_title;
        //vlc_es_id_t *m_id = NULL;
        ::vlc::vlc_shared_data_ptr<vlc_es_id_t, &vlc_es_id_Hold, &vlc_es_id_Release> m_id;
        bool m_selected = false;
        /* Several audio tracks at once: switched off by the user but still
         * playing, silent. Switching it on again is then instant, instead
         * of waiting for the track to be demuxed again. */
        bool m_muted = false;
        float m_volume = 1.f;
    };
    QList<Data> m_data;
    bool m_multiSelect = false; // subtitles and audio

    void saveEnabled() const;
    void applyRemembered();
    QSettings *m_settings = nullptr;
    QString m_settingsKey;
    /* The remembered tracks are applied once per media, not over the
     * user's own choice */
    bool m_applied = false;
};

/**
 * @brief The TitleListModel class represent the
 * titles of an input
 *
 * the model expose the title's name using the Qt::DisplayRole
 * and the current title using Qt::CheckStateRole
 * we can select a title by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class TitleListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)

public:

    TitleListModel(vlc_player_t* player, QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &) const  override;

    int rowCount(const QModelIndex & = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void setCurrent(int current);

    const vlc_player_title* getTitleAt( size_t index ) const;

    void resetTitles(vlc_player_title_list* newTitleList);

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_count; }

signals:
    void countChanged();

private:
    vlc_player_t* m_player;
    typedef ::vlc::vlc_shared_data_ptr<vlc_player_title_list, &vlc_player_title_list_Hold, &vlc_player_title_list_Release> PlayerTitleList;
    PlayerTitleList m_titleList;
    int m_current = -1;
    int m_count = 0;
};

/**
 * @brief The ChapterListModel class represent the
 * chapters of an input
 *
 * the model expose the chapter's name using the Qt::DisplayRole
 * and the current chapter using Qt::CheckStateRole
 * we can select a chapter by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 * the parent vlc_player_title isn't hold by this class, it is expected that the property is be valid or null
 */
class ChapterListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)

public:
    //user role
    enum ChapterListRoles {
        TimeRole = Qt::UserRole + 1 ,
        StartPositionRole,
        EndPositionRole
    };
public:
    ChapterListModel(vlc_player_t* player, QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &) const  override;

    int rowCount(const QModelIndex & = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    QHash<int, QByteArray> roleNames() const override;

    void setCurrent(int current);

    void resetTitle(const vlc_player_title* newTitle);

    inline int getCount() const { return (m_title == nullptr) ?  0 : m_title->chapter_count; }

    Q_INVOKABLE void selectChapter(int index);

    Q_INVOKABLE int getClosestChapterFromPos(float pos, float threshold) const;

    /* If the chapter at "pos" (0..1) is one people skip, judging by its
     * name: { kind: "opening" | "ending" | "recap" | "preview",
     * index, end (position where it ends) }, else an empty map */
    Q_INVOKABLE QVariantMap skippableAt(float pos) const;

signals:
    void countChanged();

public slots:
    QString getNameAtPosition(float pos) const;

private:
    vlc_player_t* m_player = nullptr;
    const vlc_player_title* m_title = nullptr;
    int m_current = -1;
};

/**
 * @brief The ProgramListModel class represent the
 * titles of an input
 *
 * the model expose the program's name using the Qt::DisplayRole
 * and the current program using Qt::CheckStateRole
 * we can select a program by setting the Qt::CheckStateRole to true
 *
 * this class expects to be updated by the input manager
 */
class ProgramListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ getCount NOTIFY countChanged FINAL)

public:
    ProgramListModel(vlc_player_t* player, QObject* parent = nullptr);

    Qt::ItemFlags flags(const QModelIndex &) const  override;

    int rowCount(const QModelIndex & = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    void updatePrograms(enum vlc_player_list_action action, const struct vlc_player_program* program);

    void updateProgramSelection(int programid, bool selected);

    void clear();

    QHash<int, QByteArray> roleNames() const override;

    inline int getCount() const { return m_data.size(); }

signals:
    void countChanged();

private:
    vlc_player_t* m_player;
    class Data {
    public:
        Data( const struct vlc_player_program* program );

        Data(const Data& data) = default;
        Data& operator =(const Data& data) = default;

        void update( const struct vlc_player_program* program );

        QString m_title;
        int m_id;
        bool m_selected;
        bool m_scrambled;
    };
    QList<Data> m_data;
};

#endif // INPUT_MODELS_HPP
