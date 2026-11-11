/*****************************************************************************
 * video_preview.hpp: a small muted player inside a library tile
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
#ifndef TERRIKON_VIDEO_PREVIEW_HPP
#define TERRIKON_VIDEO_PREVIEW_HPP

#include "qt.hpp"

#include <QQuickItem>
#include <QTimer>
#include <QUrl>

#include <memory>

struct vlc_player_t;
struct vlc_object_t;

namespace terrikon {

struct PreviewFrames;

/* As video sites preview their videos: a player of its own, drawing into
 * the item, muted unless asked. It is not the main player: it writes
 * nothing to the media library (no history, no position to continue
 * from), and stopping it takes no time from the interface. */
class VideoPreview : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged FINAL)
    /* Where to start, in seconds */
    Q_PROPERTY(double startTime MEMBER m_startTime FINAL)
    /* Shared by every preview and remembered, as on video sites */
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged FINAL)
    Q_PROPERTY(double position READ position NOTIFY progressChanged FINAL)
    Q_PROPERTY(double time READ time NOTIFY progressChanged FINAL)
    Q_PROPERTY(double length READ length NOTIFY progressChanged FINAL)
    Q_PROPERTY(bool hasFrame READ hasFrame NOTIFY hasFrameChanged FINAL)

public:
    explicit VideoPreview(QQuickItem *parent = nullptr);
    ~VideoPreview() override;

    static void setInterface(qt_intf_t *intf);
    /* Where the preview of a media was left (seconds), 0 if none; take
     * forgets it (opening the media starts there once). Any thread. */
    static double resumeTime(const QString &mrl, bool take = false);

    QUrl source() const { return m_source; }
    void setSource(const QUrl &source);
    bool muted() const;
    void setMuted(bool muted);
    double position() const { return m_position; }
    double time() const { return m_time; }
    double length() const { return m_length; }
    bool hasFrame() const { return m_hasFrame; }

    /* 0..1 */
    Q_INVOKABLE void seek(double position);

signals:
    void sourceChanged();
    void mutedChanged();
    void progressChanged();
    void hasFrameChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    void start();
    void stop();
    void poll();
    Q_INVOKABLE void frameReady();

    QUrl m_source;
    double m_startTime = 0.;
    vlc_player_t *m_player = nullptr;
    vlc_object_t *m_owner = nullptr;
    std::shared_ptr<PreviewFrames> m_frames;
    QTimer m_poll;
    double m_position = 0., m_time = 0., m_length = 0.;
    bool m_hasFrame = false;
    static qt_intf_t *s_intf;
};

}

#endif
