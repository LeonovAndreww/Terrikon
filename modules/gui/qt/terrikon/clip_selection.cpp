/*****************************************************************************
 * clip_selection.cpp: the part of the playing file chosen for a clip
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

#include "clip_selection.hpp"

#include <algorithm>
#include <cmath>

namespace terrikon {

double ClipSelection::clampTime(double seconds) const
{
    /* Unknown length (live streams, files still being probed): only keep
     * times positive */
    if (m_length <= 0.)
        return std::max(0., seconds);
    return std::clamp(seconds, 0., m_length);
}

void ClipSelection::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    emit activeChanged();
}

void ClipSelection::setMediaLength(double length)
{
    length = std::max(0., length);
    if (qFuzzyCompare(m_length + 1., length + 1.))
        return;
    m_length = length;
    m_start = clampTime(m_start);
    m_end = std::max(clampTime(m_end), m_start);
    emit rangeChanged();
}

void ClipSelection::setStart(double seconds)
{
    const double start = clampTime(seconds);
    double end = m_end;
    if (end - start < minLength)
        end = clampTime(start + minLength);
    /* At the very end of the media the edge cannot move further */
    const double fixedStart = std::min(start, std::max(0., end - minLength));
    if (fixedStart == m_start && end == m_end)
        return;
    m_start = fixedStart;
    m_end = end;
    emit rangeChanged();
}

void ClipSelection::setEnd(double seconds)
{
    const double end = clampTime(seconds);
    double start = m_start;
    if (end - start < minLength)
        start = clampTime(end - minLength);
    const double fixedEnd = std::max(end, start + minLength);
    const double clampedEnd = m_length > 0. ? std::min(fixedEnd, m_length) : fixedEnd;
    if (start == m_start && clampedEnd == m_end)
        return;
    m_start = start;
    m_end = clampedEnd;
    emit rangeChanged();
}

void ClipSelection::begin(double now, double length)
{
    setMediaLength(length);
    if (m_active)
        return;

    m_start = clampTime(now - defaultBefore);
    m_end = clampTime(now + defaultAfter);
    if (m_end - m_start < minLength)
        m_end = clampTime(m_start + minLength);
    emit rangeChanged();
    setActive(true);
}

QString ClipSelection::formatTime(double seconds) const
{
    /* Tenths, rounded once so 59.96 s does not print as 0:60.0 */
    const long long tenths = std::llround(std::max(0., seconds) * 10.);
    const long long minutes = tenths / 600;
    const double rest = double(tenths % 600) / 10.;
    const QString secondsText = QStringLiteral("%1").arg(rest, 4, 'f', 1, QLatin1Char('0'));
    if (minutes < 60)
        return QStringLiteral("%1:%2").arg(minutes).arg(secondsText);
    return QStringLiteral("%1:%2:%3").arg(minutes / 60)
                                       .arg(minutes % 60, 2, 10, QLatin1Char('0'))
                                       .arg(secondsText);
}

void ClipSelection::shift(double seconds)
{
    const double length = duration();
    double start = clampTime(m_start + seconds);
    if (m_length > 0. && start + length > m_length)
        start = std::max(0., m_length - length);
    if (start == m_start)
        return;
    m_start = start;
    m_end = start + length;
    emit rangeChanged();
}

}
