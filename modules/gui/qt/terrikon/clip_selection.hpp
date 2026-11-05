/*****************************************************************************
 * clip_selection.hpp: the part of the playing file chosen for a clip
 *****************************************************************************
 * Copyright (C) 2026 Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#ifndef TERRIKON_CLIP_SELECTION_HPP
#define TERRIKON_CLIP_SELECTION_HPP

#include <QObject>
#include <QString>

namespace terrikon {

/* Clip mode state shared by the seek bar brackets, the export panel and the
 * hotkeys. Times are in seconds; the range always stays inside the media
 * and at least minLength long. */
class ClipSelection : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged FINAL)
    Q_PROPERTY(double mediaLength READ mediaLength WRITE setMediaLength NOTIFY rangeChanged FINAL)
    Q_PROPERTY(double start READ start WRITE setStart NOTIFY rangeChanged FINAL)
    Q_PROPERTY(double end READ end WRITE setEnd NOTIFY rangeChanged FINAL)
    Q_PROPERTY(double duration READ duration NOTIFY rangeChanged FINAL)
    /* Vertical 9:16 clip: where the crop window is (0..1 of the room
     * left around it) and how much it is zoomed in (1..4) */
    Q_PROPERTY(bool vertical MEMBER m_vertical NOTIFY verticalChanged FINAL)
    Q_PROPERTY(double cropX READ cropX WRITE setCropX NOTIFY cropChanged FINAL)
    Q_PROPERTY(double cropY READ cropY WRITE setCropY NOTIFY cropChanged FINAL)
    Q_PROPERTY(double cropZoom READ cropZoom WRITE setCropZoom NOTIFY cropChanged FINAL)

public:
    static constexpr double minLength = 0.5;
    /* Range proposed when clip mode opens: mostly before the current
     * moment, since a replay is usually stopped right after the action */
    static constexpr double defaultBefore = 20.;
    static constexpr double defaultAfter = 10.;

    using QObject::QObject;

    bool active() const { return m_active; }
    double mediaLength() const { return m_length; }
    double start() const { return m_start; }
    double end() const { return m_end; }
    double duration() const { return m_end - m_start; }
    double cropX() const { return m_cropX; }
    double cropY() const { return m_cropY; }
    double cropZoom() const { return m_cropZoom; }
    void setCropX(double x);
    void setCropY(double y);
    void setCropZoom(double zoom);

    void setActive(bool active);
    void setMediaLength(double length);
    /* Moving one edge past the other pushes the other along */
    void setStart(double seconds);
    void setEnd(double seconds);

    /* Opens clip mode around "now" unless it is already open */
    Q_INVOKABLE void begin(double now, double length);
    Q_INVOKABLE void close() { setActive(false); }
    /* Moves the whole range, keeping its duration */
    Q_INVOKABLE void shift(double seconds);
    /* "m:ss.s", or "h:mm:ss.s" from an hour on */
    Q_INVOKABLE QString formatTime(double seconds) const;

signals:
    void activeChanged();
    void rangeChanged();
    void verticalChanged();
    void cropChanged();

private:
    double clampTime(double seconds) const;

    bool m_active = false;
    double m_length = 0.;
    double m_start = 0.;
    double m_end = 0.;
    bool m_vertical = false;
    double m_cropX = .5;
    double m_cropY = .5;
    double m_cropZoom = 1.;
};

}

#endif
