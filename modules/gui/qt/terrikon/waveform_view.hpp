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

/* One waveform per audio track of a WaveformLoader, each in its colour.
 * Two tracks (game and microphone) share a baseline, one above it and one
 * below; more tracks get a lane each. Painted with QPainter: the Qt build
 * of VLC has no QML Canvas. */
class WaveformView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(terrikon::WaveformLoader *loader READ loader WRITE setLoader NOTIFY loaderChanged FINAL)
    /* Track colours, repeated when there are more tracks */
    Q_PROPERTY(QVariantList colors READ colors WRITE setColors NOTIFY colorsChanged FINAL)
    /* Distance between bars, in pixels */
    Q_PROPERTY(qreal barStep READ barStep WRITE setBarStep NOTIFY barStepChanged FINAL)

public:
    explicit WaveformView(QQuickItem *parent = nullptr);

    WaveformLoader *loader() const { return m_loader; }
    QVariantList colors() const { return m_colors; }
    qreal barStep() const { return m_barStep; }

    void setLoader(WaveformLoader *loader);
    void setColors(const QVariantList &colors);
    void setBarStep(qreal step);

    void paint(QPainter *painter) override;

signals:
    void loaderChanged();
    void colorsChanged();
    void barStepChanged();

private:
    QPointer<WaveformLoader> m_loader;
    QVariantList m_colors;
    qreal m_barStep = 3.;
};

}

#endif
