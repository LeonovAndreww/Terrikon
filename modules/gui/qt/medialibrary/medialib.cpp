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

#include "medialib.hpp"
#include "mlhelper.hpp"
#include "playlist/playlist_controller.hpp"
#include "util/shared_input_item.hpp"
#include "terrikon/folder_tree.hpp"
#include "terrikon/video_preview.hpp"

#include <utility>
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <vlc_interface.h>
#include <algorithm>

#include <QQmlEngine>

MediaLib::MediaLib(qt_intf_t *_intf, vlc::playlist::PlaylistController* playlistController, QObject *_parent)
    : QObject( _parent )
    , m_intf( _intf )
    , m_playlistController(playlistController)
    , m_ml( vlc_ml_instance_get( _intf ) )
    , m_event_cb( nullptr, [this](vlc_ml_event_callback_t* cb ) {
        vlc_ml_event_unregister_callback( m_ml, cb );
      })
{
    m_event_cb.reset( vlc_ml_event_register_callback( m_ml, MediaLib::onMediaLibraryEvent,
                                                      this ) );
    m_runner = new ThreadRunner();
}

MediaLib::~MediaLib()
{
    m_runner->destroy();
}

static void convertMLItemToPlaylistMedias(vlc_medialibrary_t* ml, const MLItemId & itemId, const QStringList &options, QVector<vlc::playlist::Media>& medias)
{
    //invalid item
    if (itemId.id == 0)
        return;

    if (itemId.type == VLC_ML_PARENT_UNKNOWN)
    {
        SharedInputItem item( vlc_ml_get_input_item( ml, itemId.id ), false );
        if (item)
            medias.push_back(vlc::playlist::Media(item.get(), options));
    }
    else if (itemId.type == VLC_ML_PARENT_FOLDER)
    {
        // Terrikon: a folder plays with its subfolders
        terrikon::forEachFolderMedia(ml, itemId.id, [&](const vlc_ml_media_t &m) {
            // folders are shown in Videos: their audio files are not meant
            if (m.i_type == VLC_ML_MEDIA_TYPE_AUDIO)
                return true;
            SharedInputItem item(vlc_ml_get_input_item( ml, m.i_id ), false);
            if (item)
                medias.push_back(vlc::playlist::Media(item.get(), options));
            return true;
        });
    }
    else
    {
        vlc_ml_query_params_t query = vlc_ml_query_params_create();
        ml_unique_ptr<vlc_ml_media_list_t> media_list(vlc_ml_list_media_of( ml, &query, itemId.type, itemId.id));
        if (media_list == nullptr || media_list->i_nb_items == 0)
            return;

        for (vlc_ml_media_t& m : ml_range_iterate<vlc_ml_media_t>( media_list ))
        {
            SharedInputItem item(vlc_ml_get_input_item( ml, m.i_id ), false);
            // gone since the list was made: the playlist cannot take an empty media
            if (item)
                medias.push_back(vlc::playlist::Media(item.get(), options));
        }
    }
}

static void convertQUrlToPlaylistMedias(QUrl mrl, const QStringList& options, QVector<vlc::playlist::Media>& medias)
{
    vlc::playlist::Media media{ mrl.toString(QUrl::FullyEncoded), mrl.fileName(), options };
    medias.push_back(media);
}

static void convertQStringToPlaylistMedias(const QString &mrl, const QStringList& options, QVector<vlc::playlist::Media>& medias)
{
    vlc::playlist::Media media{ mrl, mrl, options };
    medias.push_back(media);
}


static void convertQVariantListToPlaylistMedias(vlc_medialibrary_t* ml, QVariantList itemIdList, const QStringList& options, QVector<vlc::playlist::Media>& medias)
{
    for (const QVariant& varValue: itemIdList)
    {
        if (varValue.canConvert<QUrl>())
        {
            auto mrl = varValue.value<QUrl>();
            convertQUrlToPlaylistMedias(mrl, options, medias);
        }
        else if (varValue.canConvert<QString>())
        {
            auto mrl = varValue.value<QString>();
            convertQStringToPlaylistMedias(mrl, options, medias);
        }
        else if (varValue.canConvert<MLItemId>())
        {
            MLItemId itemId = varValue.value<MLItemId>();
            convertMLItemToPlaylistMedias(ml, itemId, options, medias);
        }
        // else ignore
    }
}

MLItemId MediaLib::deserializeMlItemIdFromString(const QString& serialized_id) {
    return MLItemId::fromString(serialized_id);
}

void MediaLib::addToPlaylist(const QString& mrl, const QStringList &options)
{
    QVector<vlc::playlist::Media> medias;
    convertQStringToPlaylistMedias(mrl, options, medias);
    m_playlistController->append(medias, false);
}

void MediaLib::addToPlaylist(const QUrl& mrl, const QStringList &options)
{
    QVector<vlc::playlist::Media> medias;
    convertQUrlToPlaylistMedias(mrl, options, medias);
    m_playlistController->append(medias, false);
}

// A specific item has been asked to be added to the playlist
void MediaLib::addToPlaylist(const MLItemId & itemId, const QStringList &options)
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
    };
    runOnMLThread<Context>(this,
    //ML thread
    [itemId, options]
    (vlc_medialibrary_t* ml, Context& ctx){
        convertMLItemToPlaylistMedias(ml, itemId, options, ctx.medias);
    },
    //UI thread
    [this](quint64, Context& ctx){
        if (!ctx.medias.empty())
            m_playlistController->append(ctx.medias, false);
    });
}

void MediaLib::addToPlaylist(const QVariantList& itemIdList, const QStringList &options)
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
    };
    runOnMLThread<Context>(this,
    //ML thread
    [itemIdList, options]
    (vlc_medialibrary_t* ml, Context& ctx)
    {
        convertQVariantListToPlaylistMedias(ml, itemIdList, options, ctx.medias);
    },
    //UI thread
    [this](quint64, Context& ctx){
        if (!ctx.medias.empty())
            m_playlistController->append(ctx.medias, false);
    });
}

// A specific item has been asked to be played,
// so it's added to the playlist and played
void MediaLib::addAndPlay(const MLItemId & itemId, const QStringList &options )
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
    };
    runOnMLThread<Context>(this,
    //ML thread
    [itemId, options]
    (vlc_medialibrary_t* ml, Context& ctx)
    {
        convertMLItemToPlaylistMedias(ml, itemId, options, ctx.medias);
    },
    //UI thread
    [this](quint64, Context& ctx){
        if (!ctx.medias.empty())
            m_playlistController->append(ctx.medias, true);
    });


}

void MediaLib::lookUpFolder(const QUrl &url)
{
    struct Context {
        bool found = false;
        int64_t id = 0;
        QString title;
    };
    /* compared decoded: the library and Qt encode differently (%27...) */
    const auto norm = [](const char *mrl) { return QUrl::fromPercentEncoding(terrikon::folderKey(mrl).toUtf8()).toLower(); };
    const QString key = norm(url.toString(QUrl::FullyEncoded).toUtf8().constData());
    runOnMLThread<Context>(this,
    //ML thread
    [key, norm](vlc_medialibrary_t* ml, Context& ctx)
    {
        const auto look = [&](vlc_ml_folder_list_t *list) {
            ml_unique_ptr<vlc_ml_folder_list_t> owner(list);
            if (!list)
                return;
            for (const vlc_ml_folder_t &f : ml_range_iterate<vlc_ml_folder_t>(owner))
            {
                if (ctx.found || f.b_banned || !f.psz_mrl
                    || norm(f.psz_mrl) != key)
                    continue;
                ctx.found = true;
                ctx.id = f.i_id;
                ctx.title = qfu(f.psz_name);
            }
        };
        look(vlc_ml_list_entry_points(ml, nullptr));
        if (!ctx.found)
            look(vlc_ml_list_folders(ml, nullptr));
    },
    //UI thread
    [this, url](quint64, Context& ctx) {
        QString title = ctx.title;
        if (title.isEmpty())
            title = QUrl::fromPercentEncoding(url.toString(QUrl::FullyEncoded).section(QLatin1Char('/'), -1, -1, QString::SectionSkipEmpty).toUtf8());
        emit folderLookedUp(url, ctx.found, MLItemId(ctx.id, VLC_ML_PARENT_FOLDER), title);
    });
}

void MediaLib::playWithFolder(const MLItemId &itemId)
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
        int start = 0;
    };
    runOnMLThread<Context>(this,
    //ML thread: the folder may be on a slow network drive
    [itemId]
    (vlc_medialibrary_t* ml, Context& ctx)
    {
        SharedInputItem clicked(vlc_ml_get_input_item(ml, itemId.id), false);
        if (!clicked)
            return;
        /* watched a while in its preview: it opens there */
        QStringList clickedOptions;
        if (const double at = terrikon::VideoPreview::resumeTime(
                QUrl::fromPercentEncoding(QByteArray(clicked->psz_uri)), true); at > 0.)
            clickedOptions << QStringLiteral(":start-time=%1").arg(at, 0, 'f', 2);
        const QUrl url(QString::fromUtf8(clicked->psz_uri));
        if (!url.isLocalFile())
        {
            ctx.medias.push_back(vlc::playlist::Media(clicked.get(), clickedOptions));
            return;
        }
        const QFileInfo file(url.toLocalFile());
        const QStringList patterns = QString::fromLatin1(EXTENSIONS_VIDEO).split(QLatin1Char(';'),
                                                                               Qt::SkipEmptyParts);
        QFileInfoList siblings = file.dir().entryInfoList(patterns, QDir::Files | QDir::Readable);
        QCollator collator;
        collator.setNumericMode(true);
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(siblings.begin(), siblings.end(), [&](const QFileInfo &a, const QFileInfo &b) {
            return collator.compare(a.fileName(), b.fileName()) < 0;
        });
        for (const QFileInfo &sibling : siblings)
        {
            if (sibling.absoluteFilePath() == file.absoluteFilePath())
            {
                ctx.start = ctx.medias.size();
                ctx.medias.push_back(vlc::playlist::Media(clicked.get(), clickedOptions));
            }
            else
            {
                const QString mrl = QUrl::fromLocalFile(sibling.absoluteFilePath()).toString(QUrl::FullyEncoded);
                ctx.medias.push_back(vlc::playlist::Media(mrl, sibling.completeBaseName(), {}));
            }
        }
        if (ctx.medias.isEmpty())
            ctx.medias.push_back(vlc::playlist::Media(clicked.get(), clickedOptions));
    },
    //UI thread
    [this](quint64, Context& ctx){
        if (ctx.medias.empty())
            return;
        m_playlistController->clear();
        m_playlistController->append(ctx.medias, false);
        m_playlistController->goTo(ctx.start, true);
    });
}

void MediaLib::addAndPlay(const QString& mrl, const QStringList &options)
{
    vlc::playlist::Media media{ mrl, mrl, options };
    m_playlistController->append( QVector<vlc::playlist::Media>{media}, true );
}

void MediaLib::addAndPlay(const QUrl& mrl, const QStringList &options)
{
    vlc::playlist::Media media{ mrl.toString(QUrl::FullyEncoded), mrl.fileName(), options };
    m_playlistController->append( QVector<vlc::playlist::Media>{media}, true );
}


void MediaLib::addAndPlay(const QVariantList& itemIdList, const QStringList &options)
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
    };
    runOnMLThread<Context>(this,
    //ML thread
    [itemIdList, options]
    (vlc_medialibrary_t* ml, Context& ctx)
    {
        convertQVariantListToPlaylistMedias(ml, itemIdList, options, ctx.medias);
    },
    //UI thread
    [this](quint64, Context& ctx){
        if (!ctx.medias.empty())
            m_playlistController->append(ctx.medias, true);
    });
}

void MediaLib::insertIntoPlaylist(const size_t index, const QVariantList &itemIdList, const QStringList &options)
{
    struct Context {
        QVector<vlc::playlist::Media> medias;
    };
    runOnMLThread<Context>(this,
    //ML thread
    [itemIdList, options]
    (vlc_medialibrary_t* ml, Context& ctx )
    {
        convertQVariantListToPlaylistMedias(ml, itemIdList, options, ctx.medias);
    },
    //UI thread
    [this, index]
    (quint64, Context& ctx) {
        if (!ctx.medias.isEmpty())
            m_playlistController->insert( index, ctx.medias );
    });
}

void MediaLib::reload()
{
    runOnMLThread(this,
    //ML thread
    [](vlc_medialibrary_t* ml){
        vlc_ml_reload_folder(ml, nullptr);
    });
}

void MediaLib::mlInputItem(const QVector<MLItemId>& itemIdVector, QJSValue callback)
{
    if (!callback.isCallable()) // invalid argument
    {
        msg_Warn(m_intf, "callback is not callbable");
        return;
    }

    struct Ctx {
        std::vector<SharedInputItem> items;
    };

    if (itemIdVector.empty())
    {
        //call the callback with and empty list
        auto jsEngine = qjsEngine(this);
        if (!jsEngine)
            return;
        auto jsArray = jsEngine->newArray(0);
        callback.call({jsArray});
        return;
    }

    auto it = m_inputItemQuery.find(itemIdVector);

    if (it == m_inputItemQuery.end())
    {
        it = m_inputItemQuery.insert(itemIdVector, {callback});
    }
    else
    {
        // be patient

        for (const auto& it2 : it.value())
        {
            if (callback.strictlyEquals(it2))
                return;
        }

        it.value().push_back(callback); // FIXME: Use an ordered set
        return;
    }

    runOnMLThread<Ctx>(this,
    //ML thread
    [itemIdVector](vlc_medialibrary_t* ml, Ctx& ctx){
        for (auto mlId : itemIdVector)
        {
            // NOTE: When we have a parent it's a collection of media(s).
            if (mlId.type == VLC_ML_PARENT_UNKNOWN)
            {
                ctx.items.emplace_back(vlc_ml_get_input_item(ml, mlId.id), false);
            }
            else if (mlId.type == VLC_ML_PARENT_FOLDER)
            {
                terrikon::forEachFolderMedia(ml, mlId.id, [&](const vlc_ml_media_t &media) {
                    if (media.i_type == VLC_ML_MEDIA_TYPE_AUDIO)
                        return true;
                    ctx.items.emplace_back(vlc_ml_get_input_item(ml, media.i_id), false);
                    return true;
                });
            }
            else
            {
                ml_unique_ptr<vlc_ml_media_list_t> list;

                vlc_ml_query_params_t query = vlc_ml_query_params_create();

                list.reset(vlc_ml_list_media_of(ml, &query, mlId.type, mlId.id));

                if (list == nullptr)
                    continue;

                for (const vlc_ml_media_t & media : ml_range_iterate<vlc_ml_media_t>(list))
                    ctx.items.emplace_back(vlc_ml_get_input_item(ml, media.i_id), false);
            }
        }
    },
    //UI thread
    [this, itemIdVector](quint64, Ctx& ctx)
    {
        // taken out first: a callback may ask for the same items again
        const QVector<QJSValue> callbacks = m_inputItemQuery.take(itemIdVector);

        auto jsEngine = qjsEngine(this);
        if (!jsEngine)
            return;

        auto jsArray = jsEngine->newArray(ctx.items.size());

        int i  = 0;
        for (const auto& inputItem : ctx.items)
        {
            jsArray.setProperty(i, jsEngine->toScriptValue(inputItem));
            i++;
        }

        for (const auto& cb : callbacks)
        {
            cb.call({jsArray});
        }
    });
}

vlc_ml_event_callback_t* MediaLib::registerEventListener( void (*callback)(void*, const vlc_ml_event_t*), void* data)
{
    return vlc_ml_event_register_callback( m_ml , callback , data ) ;
}

void MediaLib::unregisterEventListener(vlc_ml_event_callback_t* cb)
{
    vlc_ml_event_unregister_callback( m_ml , cb );
}

void MediaLib::onMediaLibraryEvent( void* data, const vlc_ml_event_t* event )
{
    MediaLib* self = static_cast<MediaLib*>( data );
    switch ( event->i_type )
    {
        case VLC_ML_EVENT_PARSING_PROGRESS_UPDATED:
        {
            int percent =  event->parsing_progress.i_percent;
            QMetaObject::invokeMethod(self, [self, percent]() {
                self->m_parsingProgress = percent;
                self->emit parsingProgressChanged(percent);
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_STARTED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = true;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit discoveryStarted();
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_PROGRESS:
        {
            const QUrl entryPoint{ event->discovery_progress.psz_entry_point };
            const QString entryPointStr = entryPoint.isLocalFile()
                                              ? entryPoint.toLocalFile()
                                              : entryPoint.toDisplayString();

            QMetaObject::invokeMethod(self, [self, entryPointStr]() {
                self->m_discoveryEntryPoint = entryPointStr;
                self->emit discoveryEntryPointChanged(entryPointStr);
            });
            break;
        }
        case VLC_ML_EVENT_DISCOVERY_COMPLETED:
        {
            QMetaObject::invokeMethod(self, [self]() {
                self->m_discoveryPending = false;
                self->emit discoveryPendingChanged(self->m_discoveryPending);
                self->emit discoveryCompleted();
            });
            break;
        }
        case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
        {
            bool idle = event->background_idle_changed.b_idle;
            QMetaObject::invokeMethod(self, [self, idle]() {
                self->m_idle = idle;
                self->emit idleChanged();
            });
            break;
        }
        default:
            break;
    }
}

quint64 MediaLib::runOnMLThread(const QObject* obj,
                std::function< void(vlc_medialibrary_t* ml)> mlCb,
                std::function< void()> uiCb,
                const char* queue)
{
    struct NoCtx{};
    return m_runner->runOnThread<NoCtx>(obj,
    [mlCb, ml=m_ml](NoCtx&){
        mlCb(ml);
    },
    [uiCb](quint64, NoCtx&){
        uiCb();
    },
    queue);
}

quint64 MediaLib::runOnMLThread(const QObject* obj,
                std::function< void(vlc_medialibrary_t* ml)> mlCb,
                std::function< void(quint64)> uiCb, const char* queue)
{
    struct NoCtx{};
    return m_runner->runOnThread<NoCtx>(obj,
    [mlCb, ml=m_ml](NoCtx&){
        mlCb(ml);
    },
    [uiCb](quint64 requestId, NoCtx&){
        uiCb(requestId);
    },
    queue);
}

quint64 MediaLib::runOnMLThread(const QObject* obj,
                std::function< void(vlc_medialibrary_t* ml)> mlCb,
                const char* queue)
{
    struct NoCtx{};
    return m_runner->runOnThread<NoCtx>(obj,
    [mlCb, ml=m_ml](NoCtx&){
        mlCb(ml);
    },
    [](quint64, NoCtx&){
    },
    queue);
}

void MediaLib::cancelMLTask(const QObject* object, quint64 taskId)
{
    m_runner->cancelTask(object, taskId);
}

