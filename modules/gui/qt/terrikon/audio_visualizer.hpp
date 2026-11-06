/*****************************************************************************
 * audio_visualizer.hpp: Terrikon's own visualizer, drawn in the interface
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
#ifndef TERRIKON_AUDIO_VISUALIZER_HPP
#define TERRIKON_AUDIO_VISUALIZER_HPP

#include "qt.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QQuickItem>
#include <QRectF>
#include <QStringList>
#include <QVector>

struct terrikon_tap;

namespace terrikon {

/* Which visualizer and which background, kept between runs. While a style
 * is chosen, the tap filter listens to the audio. */
class Visualizer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged FINAL)
    Q_PROPERTY(int background READ background WRITE setBackground NOTIFY backgroundChanged FINAL)
    Q_PROPERTY(QStringList styleNames READ styleNames CONSTANT FINAL)
    Q_PROPERTY(QStringList backgroundNames READ backgroundNames CONSTANT FINAL)

public:
    enum Style { Off = 0, Ring, Bars, Wave, Hills, StyleCount };
    Q_ENUM(Style)
    enum Background { BlurredCover = 0, Black, Cover, BackgroundCount };
    Q_ENUM(Background)

    explicit Visualizer(qt_intf_t *intf, QObject *parent = nullptr);

    static Visualizer *instance() { return s_instance; }

    int style() const { return m_style; }
    void setStyle(int style);
    int background() const { return m_background; }
    void setBackground(int background);

    QStringList styleNames() const;
    QStringList backgroundNames() const;

    qt_intf_t *intf() const { return m_intf; }
    terrikon_tap *tap() const { return m_tap; }

signals:
    void styleChanged();
    void backgroundChanged();

private:
    void listen();

    static Visualizer *s_instance;
    qt_intf_t *m_intf;
    terrikon_tap *m_tap = nullptr;
    int m_style = Off;
    int m_background = BlurredCover;
    bool m_listening = false;
};

/* Draws what the tap heard, in step with the playing time */
class AudioVisualizer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(QColor color2 READ color2 WRITE setColor2 NOTIFY colorChanged FINAL)
    // The cover image: the ring goes around it
    Q_PROPERTY(QQuickItem *cover READ cover WRITE setCover NOTIFY coverChanged FINAL)
    Q_PROPERTY(qreal level READ level NOTIFY levelChanged FINAL)

public:
    explicit AudioVisualizer(QQuickItem *parent = nullptr);

    int style() const { return m_style; }
    void setStyle(int style);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    QColor color2() const { return m_color2; }
    void setColor2(const QColor &color);
    QQuickItem *cover() const { return m_cover; }
    void setCover(QQuickItem *cover);
    qreal level() const { return m_level; }

signals:
    void styleChanged();
    void colorChanged();
    void coverChanged();
    void levelChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void onFrame();
    void sample();

    int m_style = 0;
    QColor m_color = QColor(255, 136, 0);
    QColor m_color2 = QColor(255, 220, 160);
    QQuickItem *m_cover = nullptr;
    QRectF m_coverRect;
    qreal m_level = 0;

    QVector<float> m_bands;
    QVector<float> m_peaks;
    QVector<float> m_echo1;
    QVector<float> m_echo2;
    QVector<float> m_wave;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
    QMetaObject::Connection m_frameConnection;
};

}

#endif
