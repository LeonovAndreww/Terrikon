/*****************************************************************************
 * waveform_view.cpp: per-track waveforms drawn along the seek bar
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "waveform_view.hpp"
#include "media_tools.hpp"

#include <QColor>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace terrikon {

WaveformView::WaveformView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(false);
    connect(this, &QQuickItem::widthChanged, this, [this] { update(); });
    connect(this, &QQuickItem::heightChanged, this, [this] { update(); });
}

void WaveformView::setLoader(WaveformLoader *loader)
{
    if (m_loader == loader)
        return;
    if (m_loader)
        disconnect(m_loader, nullptr, this, nullptr);
    m_loader = loader;
    if (m_loader)
    {
        connect(m_loader, &WaveformLoader::trackLoaded, this, [this] { update(); });
        connect(m_loader, &WaveformLoader::tracksChanged, this, [this] { update(); });
    }
    emit loaderChanged();
    update();
}

void WaveformView::setColors(const QVariantList &colors)
{
    if (m_colors == colors)
        return;
    m_colors = colors;
    emit colorsChanged();
    update();
}

void WaveformView::setBarStep(qreal step)
{
    step = std::max<qreal>(1., step);
    if (qFuzzyCompare(m_barStep, step))
        return;
    m_barStep = step;
    emit barStepChanged();
    update();
}

void WaveformView::setTracks(const QVariantList &tracks)
{
    if (m_tracks == tracks)
        return;
    m_tracks = tracks;
    emit tracksChanged();
    update();
}

void WaveformView::paint(QPainter *painter)
{
    const qreal w = width();
    const qreal h = height();
    if (!m_loader || w <= 0. || h <= 0.)
        return;

    QList<int> shown;
    if (m_tracks.isEmpty())
        for (int t = 0; t < m_loader->trackCount(); ++t)
            shown << t;
    else
        for (const QVariant &track : m_tracks)
            shown << track.toInt();

    const int count = std::max(2, int(w / m_barStep));
    const qreal middle = h / 2.;
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    for (int t : shown)
    {
        if (t < 0 || t >= m_loader->trackCount() || !m_loader->isLoaded(t))
            continue;

        const QVariantList peaks = m_loader->peaks(t, count);
        if (peaks.size() < 2)
            continue;
        /* Each track is scaled to its own loudest moment: a microphone is
         * far quieter than the game and would otherwise be a flat line */
        double loudest = 0.;
        for (const QVariant &peak : peaks)
            loudest = std::max(loudest, peak.toDouble());
        const double gain = loudest > 0.001 ? 1. / loudest : 1.;

        /* Upper edge left to right, then the mirrored lower edge back */
        QPainterPath envelope;
        const qreal step = w / (peaks.size() - 1);
        for (qsizetype i = 0; i < peaks.size(); ++i)
        {
            const qreal half = std::max<qreal>(.5, peaks.at(i).toDouble() * gain * middle);
            const QPointF point(i * step, middle - half);
            if (i == 0)
                envelope.moveTo(point);
            else
                envelope.lineTo(point);
        }
        for (qsizetype i = peaks.size() - 1; i >= 0; --i)
        {
            const qreal half = std::max<qreal>(.5, peaks.at(i).toDouble() * gain * middle);
            envelope.lineTo(i * step, middle + half);
        }
        envelope.closeSubpath();

        QColor color = m_colors.isEmpty()
                     ? QColor(Qt::white)
                     : m_colors.at(t % m_colors.size()).value<QColor>();
        color.setAlphaF(.7);
        painter->fillPath(envelope, color);
    }
}

}
