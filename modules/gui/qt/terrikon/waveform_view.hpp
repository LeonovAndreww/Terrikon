/*****************************************************************************
 * waveform_view.hpp: per-track waveforms drawn along the seek bar
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
#ifndef TERRIKON_WAVEFORM_VIEW_HPP
#define TERRIKON_WAVEFORM_VIEW_HPP

#include "media_tools.hpp"

#include <QPointer>
#include <QQuickPaintedItem>
#include <QVariantList>

namespace terrikon {

/* The waveform of each shown audio track of a WaveformLoader: a filled
 * envelope around a centre line, in the track's colour, translucent so
 * overlapping tracks stay readable. Painted with QPainter: the Qt build
 * of VLC has no QML Canvas. */
class WaveformView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(terrikon::WaveformLoader *loader READ loader WRITE setLoader NOTIFY loaderChanged FINAL)
    /* Track colours, repeated when there are more tracks */
    Q_PROPERTY(QVariantList colors READ colors WRITE setColors NOTIFY colorsChanged FINAL)
    /* Horizontal resolution of the envelope, in pixels */
    Q_PROPERTY(qreal barStep READ barStep WRITE setBarStep NOTIFY barStepChanged FINAL)
    /* Indexes of the tracks to draw; all of them when empty */
    Q_PROPERTY(QVariantList tracks READ tracks WRITE setTracks NOTIFY tracksChanged FINAL)

public:
    explicit WaveformView(QQuickItem *parent = nullptr);

    WaveformLoader *loader() const { return m_loader; }
    QVariantList colors() const { return m_colors; }
    qreal barStep() const { return m_barStep; }
    QVariantList tracks() const { return m_tracks; }

    void setLoader(WaveformLoader *loader);
    void setColors(const QVariantList &colors);
    void setBarStep(qreal step);
    void setTracks(const QVariantList &tracks);

    void paint(QPainter *painter) override;

signals:
    void loaderChanged();
    void colorsChanged();
    void barStepChanged();
    void tracksChanged();

private:
    QPointer<WaveformLoader> m_loader;
    QVariantList m_colors;
    qreal m_barStep = 2.;
    QVariantList m_tracks;
};

}

#endif
