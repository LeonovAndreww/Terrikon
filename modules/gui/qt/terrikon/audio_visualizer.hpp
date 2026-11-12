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
#include <QPointer>
#include <QPoint>
#include <QSize>
#include <QQuickItem>
#include <QRectF>
#include <QStringList>
#include <QUrl>
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
    // A small window over all the others, with the cover and a ring
    Q_PROPERTY(bool miniWindow READ miniWindow WRITE setMiniWindow NOTIFY miniWindowChanged FINAL)
    Q_PROPERTY(QPoint miniPosition READ miniPosition WRITE setMiniPosition NOTIFY miniPositionChanged FINAL)
    // Card (a square cover, as music services have it), round, strip, or
    // the visualization alone
    Q_PROPERTY(int miniShape READ miniShape WRITE setMiniShape NOTIFY miniShapeChanged FINAL)
    Q_PROPERTY(QStringList miniShapeNames READ miniShapeNames CONSTANT FINAL)
    // Closed by the user: not shown on minimizing until the player is back
    Q_PROPERTY(bool miniDismissed READ miniDismissed WRITE setMiniDismissed NOTIFY miniDismissedChanged FINAL)
    // White, taken from the cover, or one of the user's
    Q_PROPERTY(int colorMode READ colorMode WRITE setColorMode NOTIFY colorsChanged FINAL)
    /* the fainter, delayed layers behind the main one (hills) */
    Q_PROPERTY(bool echoes READ echoes WRITE setEchoes NOTIFY echoesChanged FINAL)
    Q_PROPERTY(QStringList colorModeNames READ colorModeNames CONSTANT FINAL)
    Q_PROPERTY(QColor customColor READ customColor WRITE setCustomColor NOTIFY colorsChanged FINAL)
    // The colours to draw with, following the mode and the cover
    Q_PROPERTY(QColor color READ color NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor color2 READ color2 NOTIFY colorsChanged FINAL)

public:
    bool echoes() const { return m_echoes; }
    void setEchoes(bool on);
    enum Style { Off = 0, Ring, Bars, Wave, Hills, Mirror, Line, StyleCount };
    Q_ENUM(Style)
    enum Background { BlurredCover = 0, Black, Cover, BackgroundCount };
    Q_ENUM(Background)

    explicit Visualizer(qt_intf_t *intf, QObject *parent = nullptr);
    ~Visualizer() override { if (s_instance == this) s_instance = nullptr; }

    static Visualizer *instance() { return s_instance; }

    int style() const { return m_style; }
    void setStyle(int style);
    int background() const { return m_background; }
    void setBackground(int background);

    bool miniWindow() const { return m_miniWindow; }
    bool miniDismissed() const { return m_miniDismissed; }
    void setMiniDismissed(bool dismissed)
    {
        if (dismissed == m_miniDismissed)
            return;
        m_miniDismissed = dismissed;
        emit miniDismissedChanged();
    }
    void setMiniWindow(bool on);
    QPoint miniPosition() const { return m_miniPosition; }
    /* Where the mini window of this size goes: where it was left if that
     * is still on a screen (any screen, left of the main one too), else at
     * the bottom right of the main screen */
    Q_INVOKABLE QPoint miniPlace(const QSize &size) const;
    void setMiniPosition(const QPoint &position);

    enum MiniShape { Card = 0, Round, Strip, Bare, MiniShapeCount };
    Q_ENUM(MiniShape)
    int miniShape() const { return m_miniShape; }
    void setMiniShape(int shape);
    QStringList miniShapeNames() const;
    // The size of the mini window, kept for each shape
    Q_INVOKABLE QSize miniSize(int shape) const;
    Q_INVOKABLE void setMiniSize(int shape, const QSize &size);

    QStringList styleNames() const;
    QStringList backgroundNames() const;

    enum ColorMode { White = 0, CoverColor, Custom, ColorModeCount };
    Q_ENUM(ColorMode)
    int colorMode() const { return m_colorMode; }
    void setColorMode(int mode);
    QStringList colorModeNames() const;
    QColor customColor() const { return m_customColor; }
    void setCustomColor(const QColor &color);
    QColor color() const;
    QColor color2() const;
    // Asks for a colour of one's own and draws with it
    Q_INVOKABLE void pickCustomColor();

    qt_intf_t *intf() const { return m_intf; }
    terrikon_tap *tap() const { return m_tap; }

signals:
    void echoesChanged();
    void styleChanged();
    void backgroundChanged();
    void miniWindowChanged();
    void miniPositionChanged();
    void miniShapeChanged();
    void colorsChanged();
    void miniDismissedChanged();

private:
    bool m_echoes = true;
    void listen();
    void readCoverColor();

    static Visualizer *s_instance;
    qt_intf_t *m_intf;
    terrikon_tap *m_tap = nullptr;
    int m_style = Off;
    int m_background = BlurredCover;
    bool m_listening = false;
    bool m_miniWindow = false;
    QPoint m_miniPosition;
    bool m_hasMiniPosition = false;
    int m_miniShape = Card;
    int m_colorMode = White;
    bool m_miniDismissed = false;
    QColor m_customColor = QColor(0x4f, 0xc3, 0xf7);
    QColor m_coverColor;
    QUrl m_coverUrl;
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
    // Use the whole item, as in the mini window, instead of keeping to the
    // edges of the music player
    Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY fillChanged FINAL)

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
    bool fill() const { return m_fill; }
    void setFill(bool fill);

signals:
    void styleChanged();
    void colorChanged();
    void coverChanged();
    void levelChanged();
    void fillChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    void onFrame();
    void sample();

    int m_style = 0;
    QColor m_color = QColor(255, 136, 0);
    QColor m_color2 = QColor(255, 220, 160);
    QPointer<QQuickItem> m_cover;
    QRectF m_coverRect;
    qreal m_level = 0;
    bool m_fill = false;

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
