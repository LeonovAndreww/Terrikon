/*****************************************************************************
 * audio_visualizer.cpp: Terrikon's own visualizer, drawn in the interface
 *****************************************************************************
 * Copyright (C) 2026 the Terrikon authors
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

#include "audio_visualizer.hpp"
#include "player/player_controller.hpp"

#include "../../../audio_filter/terrikon_tap.h"

#include <QQuickWindow>
#include <QSettings>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>
#include <QtMath>

#include <vlc_player.h>
#include <vlc_variables.h>

namespace terrikon {

static const char styleKey[] = "Terrikon/visualizer-style";
static const char backgroundKey[] = "Terrikon/visualizer-background";
static const char miniKey[] = "Terrikon/visualizer-mini";
static const char miniPositionKey[] = "Terrikon/visualizer-mini-position";
static const char miniShapeKey[] = "Terrikon/visualizer-mini-shape";
static const char miniSizeKey[] = "Terrikon/visualizer-mini-size-%1";
static const char tapFilter[] = "terrikon_tap";

Visualizer *Visualizer::s_instance = nullptr;

Visualizer::Visualizer(qt_intf_t *intf, QObject *parent)
    : QObject(parent)
    , m_intf(intf)
{
    s_instance = this;

    /* Lives as long as the process: the filter may still be writing to it
     * while the interface goes away */
    m_tap = new terrikon_tap{};
    vlc_mutex_init(&m_tap->lock);

    libvlc_int_t *libvlc = vlc_object_instance(m_intf);
    var_Create(libvlc, TERRIKON_TAP_VAR, VLC_VAR_ADDRESS);
    var_SetAddress(libvlc, TERRIKON_TAP_VAR, m_tap);

    m_style = qBound(0, m_intf->mainSettings->value(QString::fromLatin1(styleKey), 0).toInt(),
                     StyleCount - 1);
    m_background = qBound(0, m_intf->mainSettings->value(QString::fromLatin1(backgroundKey), 0).toInt(),
                          BackgroundCount - 1);

    m_miniWindow = m_intf->mainSettings->value(QString::fromLatin1(miniKey), false).toBool();
    m_miniPosition = m_intf->mainSettings->value(QString::fromLatin1(miniPositionKey), QPoint(-1, -1)).toPoint();
    m_miniShape = qBound(0, m_intf->mainSettings->value(QString::fromLatin1(miniShapeKey), int(Card)).toInt(),
                         MiniShapeCount - 1);

    /* The audio output may only come with the first playback */
    connect(m_intf->p_mainPlayerController, &PlayerController::playingStateChanged,
            this, [this]() { if (m_listening != (m_style != Off || m_miniWindow)) listen(); });
    listen();
}

void Visualizer::setStyle(int style)
{
    style = qBound(0, style, StyleCount - 1);
    if (style == m_style)
        return;
    m_style = style;
    m_intf->mainSettings->setValue(QString::fromLatin1(styleKey), style);
    listen();
    emit styleChanged();
}

void Visualizer::setMiniWindow(bool on)
{
    if (on == m_miniWindow)
        return;
    m_miniWindow = on;
    m_intf->mainSettings->setValue(QString::fromLatin1(miniKey), on);
    listen();
    emit miniWindowChanged();
}

void Visualizer::setMiniPosition(const QPoint &position)
{
    if (position == m_miniPosition)
        return;
    m_miniPosition = position;
    m_intf->mainSettings->setValue(QString::fromLatin1(miniPositionKey), position);
    emit miniPositionChanged();
}

void Visualizer::setMiniShape(int shape)
{
    shape = qBound(0, shape, MiniShapeCount - 1);
    if (shape == m_miniShape)
        return;
    m_miniShape = shape;
    m_intf->mainSettings->setValue(QString::fromLatin1(miniShapeKey), shape);
    emit miniShapeChanged();
}

QStringList Visualizer::miniShapeNames() const
{
    return { qtr("Card"), qtr("Round"), qtr("Strip"), qtr("Visualization only") };
}

QSize Visualizer::miniSize(int shape) const
{
    static const QSize defaults[MiniShapeCount] = {
        QSize(240, 240), QSize(230, 230), QSize(380, 80), QSize(320, 90)
    };
    shape = qBound(0, shape, MiniShapeCount - 1);
    const QSize size = m_intf->mainSettings->value(QString::fromLatin1(miniSizeKey).arg(shape)).toSize();
    return size.isValid() ? size : defaults[shape];
}

void Visualizer::setMiniSize(int shape, const QSize &size)
{
    if (!size.isValid())
        return;
    m_intf->mainSettings->setValue(QString::fromLatin1(miniSizeKey).arg(qBound(0, shape, MiniShapeCount - 1)), size);
}

void Visualizer::setBackground(int background)
{
    background = qBound(0, background, BackgroundCount - 1);
    if (background == m_background)
        return;
    m_background = background;
    m_intf->mainSettings->setValue(QString::fromLatin1(backgroundKey), background);
    emit backgroundChanged();
}

QStringList Visualizer::styleNames() const
{
    return { qtr("Off"), qtr("Ring around the cover"), qtr("Bars"), qtr("Wave"), qtr("Hills"),
             qtr("Mirror"), qtr("Line") };
}

QStringList Visualizer::backgroundNames() const
{
    return { qtr("Background: blurred cover"), qtr("Background: black"), qtr("Background: cover") };
}

void Visualizer::listen()
{
    const bool want = m_style != Off || m_miniWindow;
    vlc_player_t *player = m_intf->p_mainPlayerController->getPlayer();
    if (vlc_player_aout_EnableFilter(player, tapFilter, want) == 0)
        m_listening = want;
}

/* Triangles with a colour at each corner, colours premultiplied */
namespace {

using Vertex = QSGGeometry::ColoredPoint2D;

struct Mesh
{
    QVector<Vertex> v;

    static Vertex at(QPointF p, const QColor &c)
    {
        Vertex x;
        const float a = c.alphaF();
        x.set(float(p.x()), float(p.y()),
              uchar(c.redF() * a * 255.f + .5f), uchar(c.greenF() * a * 255.f + .5f),
              uchar(c.blueF() * a * 255.f + .5f), uchar(a * 255.f + .5f));
        return x;
    }

    void quad(QPointF a, QPointF b, QPointF c, QPointF d,
              const QColor &ca, const QColor &cb, const QColor &cc, const QColor &cd)
    {
        const Vertex va = at(a, ca), vb = at(b, cb), vc = at(c, cc), vd = at(d, cd);
        v << va << vb << vc << va << vc << vd;
    }

    /* A bar from a to b, half as wide as hw, its colour going from ca to cb,
     * with a one pixel soft edge all around instead of antialiasing */
    void bar(QPointF a, QPointF b, qreal hw, const QColor &ca, const QColor &cb)
    {
        QPointF d = b - a;
        qreal len = std::hypot(d.x(), d.y());
        if (len < 0.01)
        {
            d = QPointF(0, -1);
            len = 0;
        }
        else
            d /= len;
        const QPointF n(-d.y(), d.x());
        const qreal hi = std::max(hw - .5, .15);
        const qreal ho = hi + 1;

        const QPointF a1 = a + n * hi, a2 = a - n * hi;
        const QPointF b1 = b + n * hi, b2 = b - n * hi;
        const QPointF oa1 = a - d + n * ho, oa2 = a - d - n * ho;
        const QPointF ob1 = b + d + n * ho, ob2 = b + d - n * ho;
        const QColor none(0, 0, 0, 0);

        quad(a1, b1, b2, a2, ca, cb, cb, ca);
        quad(oa1, ob1, b1, a1, none, none, cb, ca);
        quad(a2, b2, ob2, oa2, ca, cb, none, none);
        quad(oa2, oa1, a1, a2, none, none, ca, ca);
        quad(b1, ob1, ob2, b2, cb, none, none, cb);
    }
};

QColor mix(const QColor &a, const QColor &b, qreal t, qreal alpha)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(float(a.redF() + (b.redF() - a.redF()) * t),
                            float(a.greenF() + (b.greenF() - a.greenF()) * t),
                            float(a.blueF() + (b.blueF() - a.blueF()) * t),
                            float(qBound(0.0, alpha, 1.0)));
}

/* A smooth curve through the band values, n points per band */
QVector<float> smoothCurve(const QVector<float> &in, int n)
{
    QVector<float> out;
    const int count = in.size();
    if (count < 2)
        return in;
    out.reserve((count - 1) * n + 1);
    for (int i = 0; i < count - 1; i++)
    {
        const float p0 = in[std::max(i - 1, 0)], p1 = in[i];
        const float p2 = in[i + 1], p3 = in[std::min(i + 2, count - 1)];
        for (int s = 0; s < n; s++)
        {
            const float t = float(s) / n, t2 = t * t, t3 = t2 * t;
            const float v = .5f * (2 * p1 + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2
                                   + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
            out << std::max(v, 0.f);
        }
    }
    out << in.last();
    return out;
}

}

AudioVisualizer::AudioVisualizer(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    m_bands.fill(0.f, TERRIKON_TAP_BANDS);
    m_peaks.fill(0.f, TERRIKON_TAP_BANDS);
    m_echo1.fill(0.f, TERRIKON_TAP_BANDS);
    m_echo2.fill(0.f, TERRIKON_TAP_BANDS);
    m_wave.fill(0.f, TERRIKON_TAP_WAVE);
    m_clock.start();
}

void AudioVisualizer::setStyle(int style)
{
    if (style == m_style)
        return;
    m_style = style;
    emit styleChanged();
    itemChange(ItemVisibleHasChanged, ItemChangeData(isVisible()));
    update();
}

void AudioVisualizer::setFill(bool fill)
{
    if (fill == m_fill)
        return;
    m_fill = fill;
    emit fillChanged();
    update();
}

void AudioVisualizer::setColor(const QColor &color)
{
    if (color == m_color)
        return;
    m_color = color;
    emit colorChanged();
}

void AudioVisualizer::setColor2(const QColor &color)
{
    if (color == m_color2)
        return;
    m_color2 = color;
    emit colorChanged();
}

void AudioVisualizer::setCover(QQuickItem *cover)
{
    if (cover == m_cover)
        return;
    m_cover = cover;
    emit coverChanged();
}

void AudioVisualizer::itemChange(ItemChange change, const ItemChangeData &value)
{
    QQuickItem::itemChange(change, value);
    if (change != ItemSceneChange && change != ItemVisibleHasChanged)
        return;

    /* Draw again after every frame while shown, in step with the screen */
    disconnect(m_frameConnection);
    QQuickWindow *win = window();
    if (win && isVisible() && m_style != 0)
    {
        m_frameConnection = connect(win, &QQuickWindow::frameSwapped,
                                    this, &AudioVisualizer::onFrame, Qt::QueuedConnection);
        m_lastMs = m_clock.elapsed();
        update();
    }
}

void AudioVisualizer::onFrame()
{
    sample();
    update();
}

void AudioVisualizer::sample()
{
    Visualizer *settings = Visualizer::instance();
    if (!settings)
        return;

    const qint64 now = m_clock.elapsed();
    const float dt = float(std::clamp<qint64>(now - m_lastMs, 1, 100));
    m_lastMs = now;

    /* Where the picture of the cover is drawn now, in this item */
    m_coverRect = QRectF();
    if (m_cover && m_cover->isVisible())
    {
        const qreal pw = m_cover->property("paintedWidth").toReal();
        const qreal ph = m_cover->property("paintedHeight").toReal();
        if (pw > 0 && ph > 0)
            m_coverRect = mapRectFromItem(m_cover, QRectF((m_cover->width() - pw) / 2,
                                                          (m_cover->height() - ph) / 2, pw, ph));
    }

    vlc_player_t *player = settings->intf()->p_mainPlayerController->getPlayer();
    vlc_tick_t time;
    bool playing;
    {
        vlc_player_locker lock{ player };
        time = vlc_player_GetTime(player);
        playing = vlc_player_GetState(player) == VLC_PLAYER_STATE_PLAYING
               && !vlc_player_IsPaused(player);
    }

    float target[TERRIKON_TAP_BANDS] = {};
    float wave[TERRIKON_TAP_WAVE] = {};
    float level = 0.f;
    bool found = false;

    terrikon_tap *tap = settings->tap();
    if (playing && time != VLC_TICK_INVALID)
    {
        const vlc_tick_t want = time + VLC_TICK_0;
        vlc_mutex_locker lock{ &tap->lock };
        const terrikon_tap_frame *best = nullptr;
        const terrikon_tap_frame *newest = nullptr;
        for (unsigned i = 0; i < tap->count; i++)
        {
            const unsigned idx = (tap->head + TERRIKON_TAP_RING - 1 - i) % TERRIKON_TAP_RING;
            const terrikon_tap_frame *f = &tap->ring[idx];
            if (!newest)
                newest = f;
            if (f->ts != VLC_TICK_INVALID && f->ts <= want)
            {
                best = f;
                break;
            }
        }
        /* Too old to be what is heard now: the stream times are not the
         * playing times, take what is about a quarter second old */
        if (best && want - best->ts > VLC_TICK_FROM_MS(300))
            best = nullptr;
        if (!best && newest && tap->count > 12)
        {
            const bool farAhead = newest->ts == VLC_TICK_INVALID
                               || newest->ts > want + VLC_TICK_FROM_SEC(5);
            const bool farBehind = newest->ts < want - VLC_TICK_FROM_SEC(5);
            if (farAhead || farBehind)
                best = &tap->ring[(tap->head + TERRIKON_TAP_RING - 12) % TERRIKON_TAP_RING];
        }
        if (best)
        {
            std::copy(std::begin(best->bands), std::end(best->bands), target);
            std::copy(std::begin(best->wave), std::end(best->wave), wave);
            level = best->level;
            found = true;
        }
    }

    const float rise = 1.f - std::pow(.35f, dt / 16.f);
    const float fall = 1.f - std::pow(.82f, dt / 16.f);
    for (int b = 0; b < TERRIKON_TAP_BANDS; b++)
    {
        float &v = m_bands[b];
        v += (target[b] - v) * (target[b] > v ? rise : fall);
        float &p = m_peaks[b];
        p = v > p ? v : std::max(v, p - dt * .0005f);
        m_echo1[b] += (v - m_echo1[b]) * (1.f - std::pow(.93f, dt / 16.f));
        m_echo2[b] += (v - m_echo2[b]) * (1.f - std::pow(.97f, dt / 16.f));
    }
    for (int i = 0; i < TERRIKON_TAP_WAVE; i++)
        m_wave[i] = found ? m_wave[i] + (wave[i] - m_wave[i]) * .6f : m_wave[i] * .85f;

    const qreal newLevel = m_level + (level - m_level) * (level > m_level ? .5 : .1);
    if (std::abs(newLevel - m_level) > 1e-4)
    {
        m_level = newLevel;
        emit levelChanged();
    }
}

QSGNode *AudioVisualizer::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGGeometryNode *>(old);
    if (!node)
    {
        node = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);
        node->setMaterial(new QSGVertexColorMaterial);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    Mesh mesh;
    const qreal w = width(), h = height();
    const int bands = TERRIKON_TAP_BANDS;

    switch (m_style)
    {
    case Visualizer::Ring:
    {
        QPointF c(w / 2, h / 2);
        qreal r0 = std::min(w, h) * (m_fill ? .27 : .2);
        if (m_coverRect.isValid() && !m_coverRect.isEmpty())
        {
            c = m_coverRect.center();
            /* Around a square cover; across a wide one, behind its sides */
            r0 = std::sqrt(m_coverRect.width() * m_coverRect.height()) / 2 * 1.06 + 6;
        }
        const int count = bands * 2;
        const qreal step = 2 * M_PI / count;
        const qreal hw = std::max(1.2, r0 * step * .3);
        const qreal room = std::min({ c.x(), w - c.x(), c.y(), h - c.y() }) - r0 - 12;
        const qreal reach = std::min(r0 * .5, room);
        for (int i = 0; i < count; i++)
        {
            const int b = i < bands ? i : count - 1 - i;
            const float v = m_bands[b];
            const qreal angle = -M_PI / 2 + (i + .5) * step;
            const QPointF dir(std::cos(angle), std::sin(angle));
            const qreal len = 3 + v * std::max(reach, 8.0);
            mesh.bar(c + dir * r0, c + dir * (r0 + len), hw,
                     mix(m_color, m_color2, v * .3, .9), mix(m_color, m_color2, v, .95));
            const qreal pr = r0 + 3 + m_peaks[b] * std::max(reach, 8.0) + 4;
            mesh.bar(c + dir * pr, c + dir * (pr + 1), hw * .8,
                     mix(m_color2, m_color2, 0, .7), mix(m_color2, m_color2, 0, .7));
        }
        /* A thin circle that brightens with the loudness */
        const QColor line = mix(m_color2, m_color, .3, .2 + m_level * .6);
        const int seg = 180;
        for (int i = 0; i < seg; i++)
        {
            const qreal a0 = 2 * M_PI * i / seg, a1 = 2 * M_PI * (i + 1) / seg;
            const qreal r = r0 - 4;
            mesh.bar(c + QPointF(std::cos(a0), std::sin(a0)) * r,
                     c + QPointF(std::cos(a1), std::sin(a1)) * r, 1, line, line);
        }
        break;
    }
    case Visualizer::Bars:
    {
        const qreal margin = m_fill ? 3 : w * .06;
        const qreal slot = (w - 2 * margin) / bands;
        const qreal hw = std::max(.8, slot * .32);
        const qreal top = m_fill ? h - 12 : h * .24;
        const qreal base = h - 2;
        for (int b = 0; b < bands; b++)
        {
            const qreal x = margin + (b + .5) * slot;
            const float v = m_bands[b];
            const qreal y = base - 2 - v * top;
            mesh.bar(QPointF(x, base), QPointF(x, y), hw,
                     mix(m_color, m_color, 0, .4), mix(m_color, m_color2, v, .75));
            const qreal py = base - 6 - m_peaks[b] * top;
            mesh.bar(QPointF(x, py), QPointF(x, py - 1.5), hw,
                     mix(m_color2, m_color2, 0, .8), mix(m_color2, m_color2, 0, .8));
        }
        break;
    }
    case Visualizer::Wave:
    {
        const qreal amp = m_fill ? h * .3 : h * .09;
        const qreal mid = m_fill ? h / 2 : h - amp * 1.4 - 6;
        const qreal margin = m_fill ? 4 : w * .04;
        const int n = TERRIKON_TAP_WAVE;
        const qreal dx = (w - 2 * margin) / (n - 1);
        auto point = [&](int i) {
            /* Quieter towards both ends, so the line comes out of nothing */
            const qreal edge = std::sin(M_PI * i / (n - 1));
            return QPointF(margin + i * dx, mid - m_wave[i] * amp * 2.2 * edge);
        };
        const QColor glow = mix(m_color, m_color, 0, .16 + m_level * .2);
        const QColor core = mix(m_color, m_color2, m_level, .95);
        for (int i = 0; i < n - 1; i++)
            mesh.bar(point(i), point(i + 1), 5, glow, glow);
        for (int i = 0; i < n - 1; i++)
            mesh.bar(point(i), point(i + 1), 1.4, core, core);
        break;
    }
    case Visualizer::Hills:
    {
        const qreal top = m_fill ? h * .8 : h * .28;
        const qreal base = h;
        const struct { const QVector<float> *values; qreal alpha; qreal lift; } layers[] = {
            { &m_echo2, .14, 1.15 }, { &m_echo1, .22, 1.05 }, { &m_bands, .4, 1.0 },
        };
        for (const auto &layer : layers)
        {
            const QVector<float> curve = smoothCurve(*layer.values, 4);
            const int n = curve.size();
            const qreal dx = w / (n - 1);
            const QColor hi = mix(m_color, m_color2, .2, layer.alpha);
            const QColor lo = mix(m_color, m_color, 0, layer.alpha * .15);
            for (int i = 0; i < n - 1; i++)
            {
                const QPointF p0(i * dx, base - 2 - curve[i] * top * layer.lift);
                const QPointF p1((i + 1) * dx, base - 2 - curve[i + 1] * top * layer.lift);
                mesh.quad(p0, p1, QPointF(p1.x(), base), QPointF(p0.x(), base), hi, hi, lo, lo);
                mesh.bar(p0, p1, .9, mix(m_color2, m_color2, 0, layer.alpha + .2),
                         mix(m_color2, m_color2, 0, layer.alpha + .2));
            }
        }
        break;
    }
    case Visualizer::Mirror:
    {
        /* The spectrum both ways from a middle line, low in the middle */
        const qreal half = m_fill ? h / 2 - 3 : h * .11;
        const qreal mid = m_fill ? h / 2 : h - half - 6;
        const qreal margin = m_fill ? 3 : w * .06;
        const int count = bands * 2;
        const qreal slot = (w - 2 * margin) / count;
        const qreal hw = std::max(.8, slot * .3);
        for (int i = 0; i < count; i++)
        {
            const int b = i < bands ? bands - 1 - i : i - bands;
            const float v = m_bands[b];
            const qreal x = margin + (i + .5) * slot;
            const qreal len = 1 + v * half;
            const QColor tip = mix(m_color, m_color2, v, .9);
            const QColor core = mix(m_color, m_color, 0, .55);
            mesh.bar(QPointF(x, mid), QPointF(x, mid - len), hw, core, tip);
            mesh.bar(QPointF(x, mid), QPointF(x, mid + len), hw, core, tip);
        }
        break;
    }
    case Visualizer::Line:
    {
        /* The spectrum as one bare line, nothing filled */
        const qreal top = m_fill ? h - 10 : h * .26;
        const qreal base = h - 5;
        const QVector<float> curve = smoothCurve(m_bands, 4);
        const int n = curve.size();
        const qreal margin = m_fill ? 4 : w * .04;
        const qreal dx = (w - 2 * margin) / (n - 1);
        auto point = [&](int i) { return QPointF(margin + i * dx, base - curve[i] * top); };
        const QColor glow = mix(m_color, m_color, 0, .15 + m_level * .2);
        const QColor core = mix(m_color, m_color2, m_level, .95);
        for (int i = 0; i < n - 1; i++)
            mesh.bar(point(i), point(i + 1), 4, glow, glow);
        for (int i = 0; i < n - 1; i++)
            mesh.bar(point(i), point(i + 1), 1.3, core, core);
        break;
    }
    default:
        break;
    }

    QSGGeometry *geometry = node->geometry();
    geometry->allocate(mesh.v.size());
    if (!mesh.v.isEmpty())
        std::copy(mesh.v.cbegin(), mesh.v.cend(), geometry->vertexDataAsColoredPoint2D());
    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

}
