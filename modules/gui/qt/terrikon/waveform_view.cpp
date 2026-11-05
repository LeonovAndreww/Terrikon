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

void WaveformView::paint(QPainter *painter)
{
    const qreal w = width();
    const qreal h = height();
    if (!m_loader || w <= 0. || h <= 0.)
        return;

    const int tracks = m_loader->trackCount();
    const int count = std::max(1, int(w / m_barStep));
    const qreal barWidth = std::max<qreal>(1., m_barStep * .6);

    for (int t = 0; t < tracks; ++t)
    {
        if (!m_loader->isLoaded(t))
            continue;

        const QColor color = m_colors.isEmpty()
                           ? QColor(Qt::white)
                           : m_colors.at(t % m_colors.size()).value<QColor>();
        const QVariantList peaks = m_loader->peaks(t, count);
        /* Each track is scaled to its own loudest moment: a microphone is
         * far quieter than the game and would otherwise be a flat line */
        double loudest = 0.;
        for (const QVariant &peak : peaks)
            loudest = std::max(loudest, peak.toDouble());
        const double gain = loudest > 0.001 ? 1. / loudest : 1.;
        const qreal lane = tracks == 2 ? h / 2. : h / tracks;

        for (qsizetype i = 0; i < peaks.size(); ++i)
        {
            const qreal x = i * w / peaks.size();
            const qreal barHeight = std::max<qreal>(1., peaks.at(i).toDouble() * gain * lane);
            qreal top;
            if (tracks == 2)
                /* Track 0 grows up from the middle, track 1 down */
                top = t == 0 ? h / 2. - barHeight : h / 2.;
            else
                top = lane * (t + 1) - barHeight;
            painter->fillRect(QRectF(x, top, barWidth, barHeight), color);
        }
    }
}

}
