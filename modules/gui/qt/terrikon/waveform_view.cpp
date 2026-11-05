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

void WaveformView::setStyle(Style style)
{
    if (m_style == style)
        return;
    m_style = style;
    emit styleChanged();
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

    const qreal step = m_style == Bars ? std::max<qreal>(m_barStep, 4.) : m_barStep;
    const int count = std::max(2, int(w / step));
    painter->setRenderHint(QPainter::Antialiasing, true);

    /* Peaks of each track, 0..1. Each track is scaled to its own loudest
     * moment: a microphone is far quieter than the game and would
     * otherwise be a flat line */
    QList<QList<qreal>> levels;
    QList<int> drawn;
    for (int t : shown)
    {
        if (t < 0 || t >= m_loader->trackCount() || !m_loader->isLoaded(t))
            continue;
        const QVariantList peaks = m_loader->peaks(t, count);
        if (peaks.size() < 2)
            continue;
        double loudest = 0.;
        for (const QVariant &peak : peaks)
            loudest = std::max(loudest, peak.toDouble());
        const double gain = loudest > 0.001 ? 1. / loudest : 1.;
        QList<qreal> level;
        level.reserve(peaks.size());
        for (const QVariant &peak : peaks)
            level << std::clamp(peak.toDouble() * gain, 0., 1.);
        levels << level;
        drawn << t;
    }
    if (levels.isEmpty())
        return;

    auto colorOf = [this](int track) {
        return m_colors.isEmpty() ? QColor(Qt::white)
                                  : m_colors.at(track % m_colors.size()).value<QColor>();
    };

    switch (m_style)
    {
    case Bars:
    {
        /* One column per step, as loud as the loudest track there */
        const qsizetype n = levels.first().size();
        const qreal span = w / n;
        const qreal barWidth = std::max<qreal>(1.5, span / 2.);
        QColor color = colorOf(drawn.first());
        color.setAlphaF(.85);
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        for (qsizetype i = 0; i < n; ++i)
        {
            qreal level = 0.;
            for (const QList<qreal> &l : levels)
                if (i < l.size())
                    level = std::max(level, l.at(i));
            const qreal barHeight = std::max<qreal>(barWidth, level * h);
            painter->drawRoundedRect(QRectF(i * span + (span - barWidth) / 2., h - barHeight,
                                            barWidth, barHeight),
                                     barWidth / 2., barWidth / 2.);
        }
        break;
    }
    case Curve:
    {
        for (qsizetype k = levels.size() - 1; k >= 0; --k)
        {
            const QList<qreal> &level = levels.at(k);
            const qreal x = w / (level.size() - 1);
            auto point = [&](qsizetype i) {
                return QPointF(i * x, h - std::max<qreal>(.5, level.at(i) * (h - 1.)));
            };
            /* Through the midpoints, with the peaks as control points:
             * a smooth line that never overshoots */
            QPainterPath line;
            line.moveTo(point(0));
            for (qsizetype i = 1; i < level.size() - 1; ++i)
            {
                const QPointF a = point(i), b = point(i + 1);
                line.quadTo(a, (a + b) / 2.);
            }
            line.lineTo(point(level.size() - 1));

            QColor color = colorOf(drawn.at(k));
            if (k == 0)
            {
                QPainterPath area = line;
                area.lineTo(w, h);
                area.lineTo(0., h);
                area.closeSubpath();
                QColor fill = color;
                fill.setAlphaF(.35);
                painter->fillPath(area, fill);
            }
            color.setAlphaF(k == 0 ? .9 : .7);
            painter->strokePath(line, QPen(color, 1.25));
        }
        break;
    }
    case Mirror:
    {
        const qreal middle = h / 2.;
        painter->setPen(Qt::NoPen);
        for (qsizetype k = 0; k < levels.size(); ++k)
        {
            const QList<qreal> &level = levels.at(k);
            const qreal x = w / (level.size() - 1);
            /* Upper edge left to right, then the mirrored lower edge back */
            QPainterPath envelope;
            for (qsizetype i = 0; i < level.size(); ++i)
            {
                const QPointF point(i * x, middle - std::max<qreal>(.5, level.at(i) * middle));
                if (i == 0)
                    envelope.moveTo(point);
                else
                    envelope.lineTo(point);
            }
            for (qsizetype i = level.size() - 1; i >= 0; --i)
                envelope.lineTo(i * x, middle + std::max<qreal>(.5, level.at(i) * middle));
            envelope.closeSubpath();
            QColor color = colorOf(drawn.at(k));
            color.setAlphaF(.7);
            painter->fillPath(envelope, color);
        }
        break;
    }
    }
}

}
