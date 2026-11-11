/*****************************************************************************
 * video_preview.cpp: a small muted player inside a library tile
 *****************************************************************************
 * Copyright (C) 2026 Terrikon contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "video_preview.hpp"

#include <vlc_common.h>
#include <vlc_player.h>
#include <vlc_input_item.h>
#include <vlc_variables.h>

#include <QHash>
#include <QImage>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSettings>

#include <algorithm>
#include <cstring>
#include <mutex>
#include <thread>

namespace terrikon {

qt_intf_t *VideoPreview::s_intf = nullptr;

/* What the video output thread and the item share; it outlives both */
struct PreviewFrames
{
    std::mutex lock;
    QImage writing;     /* the output decodes into it */
    QImage shown;       /* a copy of the last complete frame */
    bool fresh = false;
    VideoPreview *item = nullptr; /* cleared when the item goes */
};

static const unsigned maxWidth = 640;

static unsigned setupFormat(void **opaque, char *chroma, unsigned *width, unsigned *height,
                            unsigned *pitches, unsigned *lines)
{
    auto frames = static_cast<PreviewFrames *>(*opaque);
    /* scaled down to what a tile shows, the shape kept */
    unsigned w = std::min(*width, maxWidth);
    unsigned h = *width ? (unsigned)((uint64_t)*height * w / *width) : *height;
    w &= ~1u;
    h &= ~1u;
    if (w == 0 || h == 0)
        return 0;
    /* B, G, R, A in memory: what QImage::Format_RGB32 holds */
    memcpy(chroma, "BGRA", 4);
    *width = w;
    *height = h;
    pitches[0] = w * 4;
    lines[0] = h;
    std::lock_guard<std::mutex> lock(frames->lock);
    frames->writing = QImage(int(w), int(h), QImage::Format_RGB32);
    frames->writing.fill(Qt::black);
    return 1;
}

static void *lockFrame(void *opaque, void **planes)
{
    auto frames = static_cast<PreviewFrames *>(opaque);
    planes[0] = frames->writing.bits();
    return nullptr;
}

static void displayFrame(void *opaque, void *)
{
    auto frames = static_cast<PreviewFrames *>(opaque);
    std::lock_guard<std::mutex> lock(frames->lock);
    frames->shown = frames->writing.copy();
    frames->fresh = true;
    if (frames->item)
        QMetaObject::invokeMethod(frames->item, "frameReady", Qt::QueuedConnection);
}

void VideoPreview::setInterface(qt_intf_t *intf)
{
    s_intf = intf;
}

static std::mutex s_resumeLock;
static QHash<QString, double> s_resume;

double VideoPreview::resumeTime(const QString &mrl, bool take)
{
    std::lock_guard<std::mutex> lock(s_resumeLock);
    const double t = s_resume.value(mrl, 0.);
    if (take)
        s_resume.remove(mrl);
    return t;
}

static QString keyOf(const QUrl &url)
{
    return QUrl::fromPercentEncoding(url.toString(QUrl::FullyEncoded).toUtf8());
}

VideoPreview::VideoPreview(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_poll.setInterval(250);
    connect(&m_poll, &QTimer::timeout, this, &VideoPreview::poll);
}

VideoPreview::~VideoPreview()
{
    stop();
}

static QSettings previewSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("terrikon"), QStringLiteral("library"));
}

bool VideoPreview::muted() const
{
    return previewSettings().value(QStringLiteral("Preview/muted"), true).toBool();
}

void VideoPreview::setMuted(bool muted)
{
    if (muted == this->muted())
        return;
    previewSettings().setValue(QStringLiteral("Preview/muted"), muted);
    if (m_player)
    {
        vlc_player_Lock(m_player);
        vlc_player_aout_Mute(m_player, muted);
        vlc_player_Unlock(m_player);
    }
    emit mutedChanged();
}

void VideoPreview::setSource(const QUrl &source)
{
    if (source == m_source)
        return;
    stop();
    m_source = source;
    m_startTime = 0.;
    if (!m_source.isEmpty())
        start();
    emit sourceChanged();
}

void VideoPreview::start()
{
    if (!s_intf)
        return;
    /* The player's settings go on an object of its own, its parent, as
     * libvlc does for its players: the output into memory, no subtitles,
     * decoding on the processor (frames come back to memory anyway). Its
     * parent not being the libvlc instance, it is a secondary player: it
     * writes nothing to the media library. */
    vlc_object_t *owner = static_cast<vlc_object_t *>(vlc_object_create(VLC_OBJECT(s_intf), sizeof(vlc_object_t)));
    if (!owner)
        return;
    m_frames = std::make_shared<PreviewFrames>();
    m_frames->item = this;
    var_Create(owner, "vout", VLC_VAR_STRING);
    var_SetString(owner, "vout", "vmem");
    var_Create(owner, "vmem-setup", VLC_VAR_ADDRESS);
    var_SetAddress(owner, "vmem-setup", (void *)setupFormat);
    var_Create(owner, "vmem-lock", VLC_VAR_ADDRESS);
    var_SetAddress(owner, "vmem-lock", (void *)lockFrame);
    var_Create(owner, "vmem-display", VLC_VAR_ADDRESS);
    var_SetAddress(owner, "vmem-display", (void *)displayFrame);
    var_Create(owner, "vmem-data", VLC_VAR_ADDRESS);
    var_SetAddress(owner, "vmem-data", m_frames.get());
    var_Create(owner, "spu", VLC_VAR_BOOL);
    var_SetBool(owner, "spu", false);
    var_Create(owner, "dec-dev", VLC_VAR_STRING);
    var_SetString(owner, "dec-dev", "none");
    /* the main player's video filters and visualizations are not for it */
    var_Create(owner, "video-filter", VLC_VAR_STRING);
    var_Create(owner, "audio-visual", VLC_VAR_STRING);

    vlc_player_t *player = vlc_player_New(owner, VLC_PLAYER_LOCK_NORMAL);
    if (!player)
    {
        vlc_object_delete(owner);
        m_frames.reset();
        return;
    }
    m_owner = owner;

    const QByteArray mrl = m_source.toString(QUrl::FullyEncoded).toUtf8();
    input_item_t *item = input_item_New(mrl.constData(), nullptr);
    if (!item)
    {
        vlc_player_Delete(player);
        vlc_object_delete(owner);
        m_owner = nullptr;
        m_frames.reset();
        return;
    }
    /* back where the last preview of it was left */
    if (m_startTime <= 0.)
        m_startTime = resumeTime(keyOf(m_source));
    if (m_startTime > 0.)
    {
        const QByteArray option = QStringLiteral(":start-time=%1").arg(m_startTime, 0, 'f', 2).toUtf8();
        input_item_AddOption(item, option.constData(), VLC_INPUT_OPTION_TRUSTED);
    }
    /* no window, no fullscreen: only this item shows it */
    input_item_AddOption(item, ":no-video-title-show", VLC_INPUT_OPTION_TRUSTED);
    /* over and over, as long as it is pointed at */
    input_item_AddOption(item, ":input-repeat=65535", VLC_INPUT_OPTION_TRUSTED);

    vlc_player_Lock(player);
    vlc_player_aout_Mute(player, muted());
    vlc_player_SetCurrentMedia(player, item);
    vlc_player_Start(player);
    vlc_player_Unlock(player);
    input_item_Release(item);

    m_player = player;
    m_poll.start();
}

void VideoPreview::stop()
{
    m_poll.stop();
    if (m_player)
    {
        /* where it was left: the next preview and opening it go on there;
         * near either end, nothing to go on from */
        poll();
        std::lock_guard<std::mutex> lock(s_resumeLock);
        const QString key = keyOf(m_source);
        if (m_time > 1. && (m_length <= 0. || m_time < m_length - 2.))
            s_resume.insert(key, m_time);
        else
            s_resume.remove(key);
    }
    if (m_frames)
    {
        std::lock_guard<std::mutex> lock(m_frames->lock);
        m_frames->item = nullptr;
    }
    if (m_player)
    {
        /* Stopping waits for the input to end (a slow drive...): not on the
         * interface thread. The frames stay alive until the output is gone. */
        vlc_player_t *player = m_player;
        vlc_object_t *owner = m_owner;
        std::shared_ptr<PreviewFrames> frames = m_frames;
        m_player = nullptr;
        m_owner = nullptr;
        std::thread([player, owner, frames] {
            vlc_player_Lock(player);
            vlc_player_Stop(player);
            vlc_player_Unlock(player);
            vlc_player_Delete(player);
            vlc_object_delete(owner);
            (void) frames;
        }).detach();
    }
    m_frames.reset();
    if (m_hasFrame)
    {
        m_hasFrame = false;
        emit hasFrameChanged();
    }
    update();
}

void VideoPreview::seek(double position)
{
    if (!m_player)
        return;
    vlc_player_Lock(m_player);
    vlc_player_SetPosition(m_player, std::clamp(position, 0., 1.));
    if (!vlc_player_IsStarted(m_player))
        vlc_player_Start(m_player);
    vlc_player_Unlock(m_player);
    poll();
}

void VideoPreview::poll()
{
    if (!m_player)
        return;
    vlc_player_Lock(m_player);
    const double position = vlc_player_GetPosition(m_player);
    const vlc_tick_t time = vlc_player_GetTime(m_player);
    const vlc_tick_t length = vlc_player_GetLength(m_player);
    vlc_player_Unlock(m_player);
    m_position = position >= 0. ? position : 0.;
    m_time = time != VLC_TICK_INVALID ? secf_from_vlc_tick(time) : 0.;
    if (length != VLC_TICK_INVALID && length > 0)
        m_length = secf_from_vlc_tick(length); /* kept between repeats */
    emit progressChanged();
}

void VideoPreview::frameReady()
{
    if (!m_hasFrame)
    {
        m_hasFrame = true;
        emit hasFrameChanged();
    }
    update();
}

QSGNode *VideoPreview::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto node = static_cast<QSGSimpleTextureNode *>(old);
    QImage image;
    if (m_frames)
    {
        std::lock_guard<std::mutex> lock(m_frames->lock);
        if (m_frames->fresh)
        {
            image = m_frames->shown;
            m_frames->fresh = false;
        }
    }
    if (!m_hasFrame)
    {
        delete node;
        return nullptr;
    }
    if (!node)
    {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Linear);
    }
    if (!image.isNull())
        node->setTexture(window()->createTextureFromImage(image));
    if (!node->texture())
    {
        delete node;
        return nullptr;
    }
    /* filling the tile, the overflow cut off, as the cover */
    const QSizeF tex = node->texture()->textureSize();
    const QRectF target = boundingRect();
    QRectF source(QPointF(0, 0), tex);
    if (tex.width() > 0 && tex.height() > 0 && target.height() > 0)
    {
        const double ratio = target.width() / target.height();
        if (tex.width() / tex.height() > ratio)
        {
            const double w = tex.height() * ratio;
            source = QRectF((tex.width() - w) / 2, 0, w, tex.height());
        }
        else
        {
            const double h = tex.width() / ratio;
            source = QRectF(0, (tex.height() - h) / 2, tex.width(), h);
        }
    }
    node->setRect(target);
    node->setSourceRect(source);
    return node;
}

}
