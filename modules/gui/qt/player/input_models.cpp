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
#include "input_models.hpp"
#include <QSettings>

#include "player/player_controller.hpp"

#include <QRegularExpression>
#include <QTimer>

#include <algorithm>

//***************************
//  track list model
//***************************

TrackListModel::TrackListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TrackListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected && !m_data[row].m_muted);
    else if (role == VolumeRole)
        return QVariant::fromValue<float>(m_data[row].m_volume);
    return QVariant{};
}

void TrackListModel::addSilentTracks()
{
    if (!m_multiSelect)
        return;
    const bool playing = std::any_of(m_data.cbegin(), m_data.cend(), [](const Data &track) {
        return track.m_selected && !track.m_muted;
    });
    if (!playing)
        return;
    {
        vlc_player_locker lock{ m_player };
        for (int i = 0; i < getCount(); i++)
            selectMuted(i);
    }
    /* Once the player has said which tracks it now plays */
    if (m_settings && !m_applied)
        QTimer::singleShot(400, this, &TrackListModel::applyRemembered);
}

void TrackListModel::remember(QSettings *settings, const QString &key)
{
    m_settings = settings;
    m_settingsKey = key;
    if (m_settings)
        setMultiSelect(m_settings->value(m_settingsKey + QStringLiteral("-several"), false).toBool());
}

void TrackListModel::saveEnabled() const
{
    if (!m_settings || !m_multiSelect)
        return;
    QStringList titles;
    for (const Data &track : m_data)
        if (track.m_selected && !track.m_muted)
            titles << track.m_title;
    if (!titles.isEmpty())
        m_settings->setValue(m_settingsKey + QStringLiteral("-on"), titles);
}

/* Switches on the tracks named as the ones on last time, off the others,
 * when at least one name matches: the recordings of one game have the same
 * tracks ("Game", "Mic"), other media are left as the player chose */
void TrackListModel::applyRemembered()
{
    if (!m_settings || !m_multiSelect || m_applied || m_data.isEmpty())
        return;
    const QStringList titles = m_settings->value(m_settingsKey + QStringLiteral("-on")).toStringList();
    const bool matches = std::any_of(m_data.cbegin(), m_data.cend(), [&](const Data &track) {
        return titles.contains(track.m_title);
    });
    m_applied = true;
    if (!matches)
        return;
    QSettings *settings = m_settings;
    m_settings = nullptr; /* not saved back while applying */
    for (int i = 0; i < m_data.size(); ++i)
    {
        const bool want = titles.contains(m_data[i].m_title);
        const bool on = m_data[i].m_selected && !m_data[i].m_muted;
        if (want != on && m_data[i].m_selected)
            setData(index(i), want, Qt::CheckStateRole);
    }
    m_settings = settings;
}

void TrackListModel::selectMuted(int row)
{
    Data &track = m_data[row];
    if (track.m_selected || vlc_es_id_GetCat(track.m_id.get()) != AUDIO_ES)
        return;
    track.m_muted = true;
    vlc_player_SetEsIdVolume(m_player, track.m_id.get(), 0.f);
    vlc_player_SelectEsId(m_player, track.m_id.get(), VLC_PLAYER_SELECT_SIMULTANEOUS);
}

QVariantList TrackListModel::trackStates() const
{
    QVariantList states;
    states.reserve(m_data.size());
    for (qsizetype i = 0; i < m_data.size(); ++i)
    {
        states.append(QVariantMap{
            { QStringLiteral("index"), int(i) },
            { QStringLiteral("title"), m_data[i].m_title },
            { QStringLiteral("enabled"), m_data[i].m_selected && !m_data[i].m_muted },
            { QStringLiteral("volume"), double(m_data[i].m_volume) },
        });
    }
    return states;
}

bool TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role == VolumeRole )
    {
        if (!value.canConvert<float>())
            return false;
        float volume = value.toFloat();
        if (m_data[row].m_muted)
            /* Kept for when the track is switched on again */
            m_data[row].m_volume = std::clamp(volume, 0.f, 2.f);
        else
        {
            vlc_player_locker lock{ m_player };
            if (vlc_player_SetEsIdVolume(m_player, m_data[row].m_id.get(), volume) != VLC_SUCCESS)
                return false;
            m_data[row].m_volume = vlc_player_GetEsIdVolume(m_player, m_data[row].m_id.get());
        }
        emit dataChanged(index, index, { VolumeRole });
        return true;
    }
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    vlc_player_locker lock{ m_player };

    Data &track = m_data[row];
    const bool audio = vlc_es_id_GetCat(track.m_id.get()) == AUDIO_ES;
    if (m_multiSelect && audio && track.m_selected)
    {
        /* Several at once: an audio track keeps playing, only its volume
         * goes to zero and back */
        if (track.m_muted == !select)
            return true;
        track.m_muted = !select;
        vlc_player_SetEsIdVolume(m_player, track.m_id.get(), select ? track.m_volume : 0.f);
        emit dataChanged(index, index, { Qt::CheckStateRole });
        saveEnabled();
        return true;
    }

    if (select)
    {
        vlc_es_id_t *es_id = m_data[row].m_id.get();
        const enum es_format_category_e cat = vlc_es_id_GetCat(es_id);
        enum vlc_player_select_policy policy =
            ( cat ==  VIDEO_ES ) || m_multiSelect ? VLC_PLAYER_SELECT_SIMULTANEOUS
                                                  : VLC_PLAYER_SELECT_EXCLUSIVE;
        vlc_player_SelectEsId(m_player, es_id, policy);
    }
    else
        vlc_player_UnselectEsId(m_player, m_data[row].m_id.get());
    return true;
}

void TrackListModel::updateTracks(vlc_player_list_action action, const vlc_player_track *track_info)
{
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
    {
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ track_info });
        endInsertRows();
        emit countChanged();
        if (m_multiSelect)
            /* Once the player has picked the track it plays */
            QTimer::singleShot(500, this, &TrackListModel::addSilentTracks);
        break;
    }
    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id.get() == track_info->es_id;
        });
        if (it == m_data.end())
            return;

        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        /* All gone: the next media gets the remembered tracks again */
        if (m_data.isEmpty())
            m_applied = false;
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id.get() == track_info->es_id)
            {
                d.update(track_info);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        break;
    }
    }
}

void TrackListModel::updateTrackSelection(vlc_es_id_t *trackid, bool selected)
{
    if (trackid == NULL)
        return;
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& track) {
        return trackid == track.m_id.get();
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    if (!selected && it->m_muted)
    {
        /* Dropped by the player: it starts at its own volume next time */
        it->m_muted = false;
        vlc_player_locker lock{ m_player };
        vlc_player_SetEsIdVolume(m_player, trackid, it->m_volume);
    }
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });
}

void TrackListModel::clear()
{
    m_applied = false;
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
    emit countChanged();
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    roleNames[VolumeRole] = QByteArrayLiteral("volume");
    return roleNames;
}

void TrackListModel::setMultiSelect(bool multiSelect)
{
    if (m_multiSelect == multiSelect)
        return;

    m_multiSelect = multiSelect;
    if ( m_multiSelect )
    {
        /* Every audio track plays from now on, the ones not wanted
         * silent: switching them on is then instant */
        vlc_player_locker lock{ m_player };
        for ( int i = 0; i < getCount(); i++ )
            selectMuted( i );
    }
    if ( !m_multiSelect && getCount() > 1 )
    {
        /* The silent ones go first, so that the one heard is kept */
        {
            vlc_player_locker lock{ m_player };
            for ( Data &track : m_data )
            {
                if ( track.m_muted )
                {
                    track.m_muted = false;
                    vlc_player_SetEsIdVolume( m_player, track.m_id.get(), track.m_volume );
                    vlc_player_UnselectEsId( m_player, track.m_id.get() );
                }
            }
        }
        int firstSelectedIndex = -1;
        for ( int i = 0; i < getCount(); i++ )
        {
            if ( data( index( i ), Qt::CheckStateRole ).toBool() )
            {
                if (firstSelectedIndex != -1)
                    setData( index(i), false, Qt::CheckStateRole );
                else
                    firstSelectedIndex = i;
            }
        }

        if ( firstSelectedIndex != -1 )
            setData( index(firstSelectedIndex), true, Qt::CheckStateRole );
    }
    if (m_settings)
        m_settings->setValue(m_settingsKey + QStringLiteral("-several"), m_multiSelect);
    emit multiSelectChanged(m_multiSelect);
}

TrackListModel::Data::Data(const vlc_player_track *track_info)
    : m_title( qfu(track_info->name) )
    , m_id( track_info->es_id, true )
    , m_selected( track_info->selected )
{
}

void TrackListModel::Data::update(const vlc_player_track *track_info)
{
    m_id.reset(track_info->es_id, true);
    m_title = qfu(track_info->name);
    m_selected = track_info->selected;
}


//***************************
//  TitleListModel
//***************************


TitleListModel::TitleListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags TitleListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int TitleListModel::rowCount(const QModelIndex &) const
{
    return m_count;
}

QVariant TitleListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_count)
        return QVariant{};
    const vlc_player_title* title = getTitleAt(row);

    if (role == Qt::DisplayRole)
        return qfu(title->name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    return QVariant{};
}

bool TitleListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row < 0 || row >= m_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        const vlc_player_title* title = getTitleAt(row);
        if (!title)
            return false;
        vlc_player_SelectTitle(m_player, title);
    }
    return true;
}

void TitleListModel::setCurrent(int current)
{
    if (m_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

const vlc_player_title *TitleListModel::getTitleAt(size_t index) const
{
    return vlc_player_title_list_GetAt(m_titleList.get(), index);
}

void TitleListModel::resetTitles(vlc_player_title_list *newTitleList)
{
    beginResetModel();
    m_titleList.reset(newTitleList, true);
    m_current = -1;
    if (m_titleList)
        m_count = vlc_player_title_list_GetCount(m_titleList.get());
    else
        m_count = 0;
    endResetModel();
    emit countChanged();
}

QHash<int, QByteArray> TitleListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    return roleNames;
}

//***************************
//  ChapterListModel
//***************************

ChapterListModel::ChapterListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ChapterListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ChapterListModel::rowCount(const QModelIndex &) const
{
    return m_title ? m_title->chapter_count : 0;
}

QVariant ChapterListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (m_title == nullptr || row < 0 || (size_t)row >= m_title->chapter_count)
        return QVariant{};
    const vlc_player_chapter& chapter = m_title->chapters[row];

    if (role == Qt::DisplayRole)
        return qfu(chapter.name);
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(row == m_current);
    else if (role == ChapterListRoles::TimeRole )
        return QVariant::fromValue<VLCTime>(chapter.time);
    else if (role == ChapterListRoles::StartPositionRole && (m_title->length != 0))
        return QVariant::fromValue<float>(chapter.time / (float) m_title->length);
    else if (role == ChapterListRoles::EndPositionRole && (m_title->length != 0))
        return (size_t)row + 1 == m_title->chapter_count
                ? QVariant::fromValue<float> (1)
                : QVariant::fromValue<float> (m_title->chapters[row+1].time / (float) m_title->length);

    return QVariant{};
}

bool ChapterListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (m_title == nullptr || (size_t)row >= m_title->chapter_count)
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    if (select)
    {
        vlc_player_locker lock{ m_player };
        vlc_player_SelectChapter(m_player, m_title, row);
    }
    return true;
}

QHash<int, QByteArray> ChapterListModel::roleNames() const
{
    return QHash<int, QByteArray>{
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {Qt::CheckStateRole, QByteArrayLiteral("checked")},
        {ChapterListRoles::TimeRole, QByteArrayLiteral("time")},
        {ChapterListRoles::StartPositionRole, QByteArrayLiteral("startPosition")},
        {ChapterListRoles::EndPositionRole, QByteArrayLiteral("endPosition")}
    };
}

void ChapterListModel::setCurrent(int current)
{
    if (m_title == nullptr || m_title->chapter_count == 0 || m_current == current)
        return;
    int oldCurrent = m_current;
    m_current = current;

    QModelIndex oldIndex = index(oldCurrent);
    QModelIndex currentIndex = index(current);

    if (oldCurrent >= 0)
        emit dataChanged(oldIndex, oldIndex, { Qt::CheckStateRole });
    if ( current >= 0 )
        emit dataChanged(currentIndex, currentIndex, { Qt::CheckStateRole });
}

void ChapterListModel::resetTitle(const vlc_player_title *newTitle)
{
    beginResetModel();
    m_title =newTitle;
    m_current = -1;
    endResetModel();
    emit countChanged();
}

void ChapterListModel::selectChapter(int index)
{
    if (!m_title || index < 0 || static_cast<size_t>(index) >= m_title->chapter_count)
        return;
    {
        vlc_player_locker lock{ m_player };
        vlc_player_SelectChapter(m_player, m_title, index);
    }
}

/* Chapter names as release groups and encoders write them, in English and
 * Russian: "OP", "Opening", "OP1 - Title", "Intro", "Опенинг",
 * "ED", "Ending", "End Credits", "Титры"... A name only has to start
 * with one of them. */
static QString skippableKind(const QString &name)
{
    static const struct { const char *kind; QRegularExpression pattern; } kinds[] = {
        { "opening", QRegularExpression(
            QStringLiteral("^\\s*(op|opening|intro|опенинг|вступление|заставка)(\\d|\\b)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption) },
        { "ending", QRegularExpression(
            QStringLiteral("^\\s*(ed|ending|outro|end credits|credits|эндинг|титры|концовка)(\\d|\\b)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption) },
        { "recap", QRegularExpression(
            QStringLiteral("^\\s*(recap|previously|ранее|в предыдущ)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption) },
        { "preview", QRegularExpression(
            QStringLiteral("^\\s*(preview|next episode|next time|превью|анонс|в следующ)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::UseUnicodePropertiesOption) },
    };
    for (const auto &entry : kinds)
        if (entry.pattern.match(name).hasMatch())
            return QString::fromLatin1(entry.kind);
    return {};
}

QVariantMap ChapterListModel::skippableAt(float pos) const
{
    if (m_title == nullptr || m_title->chapter_count == 0 || m_title->length == 0)
        return {};

    const vlc_tick_t time = pos * m_title->length;
    size_t current = 0;
    while (current + 1 < m_title->chapter_count && m_title->chapters[current + 1].time <= time)
        ++current;

    const QString kind = skippableKind(qfu(m_title->chapters[current].name));
    if (kind.isEmpty())
        return {};

    const float end = current + 1 < m_title->chapter_count
                      ? m_title->chapters[current + 1].time / float(m_title->length)
                      : 1.f;
    return {
        { QStringLiteral("kind"), kind },
        { QStringLiteral("index"), int(current) },
        { QStringLiteral("end"), end },
    };
}

QString ChapterListModel::getNameAtPosition(float pos) const
{
    if(m_title != nullptr){

        vlc_tick_t posTime = pos * m_title->length;
        int prevChapterIndex = 0;

        for(unsigned int i=0;i<m_title->chapter_count;i++){

            vlc_tick_t currentChapterTime = m_title->chapters[i].time;

            if(currentChapterTime > posTime)
                return qfu(m_title->chapters[prevChapterIndex].name);

            else if(i == (m_title->chapter_count - 1))
                return qfu(m_title->chapters[i].name);

            prevChapterIndex = i;
        }
    }
    return QString();
}

int ChapterListModel::getClosestChapterFromPos(float pos, float threshold) const
{
    if(m_title == nullptr)
        return -1;

    vlc_tick_t posTime = pos * m_title->length;
    vlc_tick_t closestTime = VLC_TICK_INVALID;
    int chapterIndex = -1;

    for(size_t i=0; i<m_title->chapter_count; i++){

        vlc_tick_t currentChapterTime = m_title->chapters[i].time;
        if (currentChapterTime == VLC_TICK_INVALID)
            continue;
        if (closestTime == VLC_TICK_INVALID
            || qAbs(currentChapterTime - posTime) < qAbs(closestTime - posTime))
        {
            closestTime = currentChapterTime;
            chapterIndex = i;
        }
        else
            break;
    }

    if (closestTime == VLC_TICK_INVALID)
        return -1;
    else
    {
        float chapterPos = closestTime / (float)m_title->length;

        if (qAbs(chapterPos - pos) < threshold)
            return chapterIndex;
        return -1;
    }

}


//***************************
//  ProgramListModel
//***************************


ProgramListModel::ProgramListModel(vlc_player_t *player, QObject *parent)
    : QAbstractListModel(parent)
    , m_player(player)
{
}

Qt::ItemFlags ProgramListModel::flags(const QModelIndex &) const
{
    return Qt::ItemIsUserCheckable;
}

int ProgramListModel::rowCount(const QModelIndex &) const
{
    return m_data.size();
}

QVariant ProgramListModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= m_data.size())
        return QVariant{};
    if (role == Qt::DisplayRole)
        return m_data[row].m_title;
    else if (role == Qt::CheckStateRole)
        return QVariant::fromValue<bool>(m_data[row].m_selected);
    return QVariant{};
}

bool ProgramListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    int row = index.row();
    if (row >= m_data.size())
        return false;
    if ( role != Qt::CheckStateRole )
        return false;
    if (!value.canConvert<bool>())
        return false;
    bool select = value.toBool();
    vlc_player_locker lock{ m_player };

    if (select)
        vlc_player_SelectProgram(m_player, m_data[row].m_id);
    return true;
}

void ProgramListModel::updatePrograms(vlc_player_list_action action, const vlc_player_program *program)
{
    assert(program);
    switch (action) {
    case VLC_PLAYER_LIST_ADDED:
        beginInsertRows({}, m_data.size(), m_data.size());
        m_data.append(Data{ program });
        endInsertRows();
        emit countChanged();
        break;

    case VLC_PLAYER_LIST_REMOVED:
    {
        auto it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& t) {
            return t.m_id == program->group_id;
        });
        if (it == m_data.end())
            return;
        int pos = std::distance(m_data.begin(), it);
        beginRemoveRows({}, pos, pos);
        m_data.erase(it);
        endRemoveRows();
        emit countChanged();
        break;
    }
    case VLC_PLAYER_LIST_UPDATED:
    {
        int pos = 0;
        bool found = false;
        for (Data& d : m_data)
        {
            if (d.m_id == program->group_id)
            {
                d.update(program);
                found = true;
                break;
            }
            pos++;
        }
        if (!found)
            return;
        QModelIndex dataIndex = index(pos);
        emit dataChanged(dataIndex, dataIndex, { Qt::DisplayRole, Qt::CheckStateRole });
        break;
    }
    }
}

void ProgramListModel::updateProgramSelection(int programid, bool selected)
{
    QList<Data>::iterator it = std::find_if(m_data.begin(), m_data.end(), [&](const Data& program) {
        return programid == program.m_id;
    });
    if (it == m_data.end())
        return;
    size_t pos = std::distance(m_data.begin(), it);
    it->m_selected = selected;
    QModelIndex dataIndex = index(pos);
    emit dataChanged(dataIndex, dataIndex, {  Qt::CheckStateRole });
}

void ProgramListModel::clear()
{
    beginRemoveRows({}, 0, m_data.size() - 1);
    m_data.clear();
    endRemoveRows();
}

QHash<int, QByteArray> ProgramListModel::roleNames() const
{
    QHash<int, QByteArray> roleNames = this->QAbstractListModel::roleNames();
    roleNames[Qt::CheckStateRole] = QByteArrayLiteral("checked");
    return roleNames;
}

ProgramListModel::Data::Data(const vlc_player_program *program)
    : m_title(qfu(program->name))
    , m_id( program->group_id )
    , m_selected( program->selected )
    , m_scrambled( program->scrambled )
{
    assert(program);
}

void ProgramListModel::Data::update(const vlc_player_program *program)
{
    assert(program);
    m_title = qfu(program->name);
    m_id = program->group_id;
    m_selected = program->selected;
    m_scrambled = program->scrambled;
}

