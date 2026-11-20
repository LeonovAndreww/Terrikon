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
#include <QVariant>
#include <QElapsedTimer>
#include <QTimer>
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
    // The cover cut to a circle, inside the ring
    Q_PROPERTY(bool roundCover READ roundCover WRITE setRoundCover NOTIFY roundCoverChanged FINAL)
    // Where the settings panel was moved to: stuck to the time line, or so
    // far from the bottom of the player (-1: where it was opened from)
    Q_PROPERTY(bool panelDocked READ panelDocked WRITE setPanelDocked NOTIFY panelPlaceChanged FINAL)
    Q_PROPERTY(qreal panelBottomGap READ panelBottomGap WRITE setPanelBottomGap NOTIFY panelPlaceChanged FINAL)
    Q_PROPERTY(QStringList colorModeNames READ colorModeNames CONSTANT FINAL)
    Q_PROPERTY(QColor customColor READ customColor WRITE setCustomColor NOTIFY colorsChanged FINAL)
    // The colours to draw with, following the mode and the cover
    Q_PROPERTY(QColor color READ color NOTIFY colorsChanged FINAL)
    Q_PROPERTY(QColor color2 READ color2 NOTIFY colorsChanged FINAL)
    // The colours along the drawing, in a gradient mode; empty otherwise
    Q_PROPERTY(QVariantList gradient READ gradient NOTIFY colorsChanged FINAL)

public:
    bool echoes() const { return m_echoes; }
    bool roundCover() const { return m_roundCover; }
    bool panelDocked() const { return m_panelDocked; }
    void setPanelDocked(bool on);
    qreal panelBottomGap() const { return m_panelBottomGap; }
    void setPanelBottomGap(qreal gap);
    void setRoundCover(bool on);
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

    enum ColorMode { White = 0, CoverColor, Custom, CoverGradient, CustomGradient, ColorModeCount };
    Q_ENUM(ColorMode)
    int colorMode() const { return m_colorMode; }
    void setColorMode(int mode);
    QStringList colorModeNames() const;
    QColor customColor() const { return m_customColor; }
    void setCustomColor(const QColor &color);
    QColor color() const;
    QColor color2() const;
    QVariantList gradient() const;
    // The two colours of the user's gradient, one dialog after the other
    Q_INVOKABLE void pickCustomGradient();
    // Asks for a colour of one's own and draws with it
    Q_INVOKABLE void pickCustomColor();

    qt_intf_t *intf() const { return m_intf; }
    terrikon_tap *tap() const { return m_tap; }

signals:
    void echoesChanged();
    void roundCoverChanged();
    void panelPlaceChanged();
    void styleChanged();
    void backgroundChanged();
    void miniWindowChanged();
    void miniPositionChanged();
    void miniShapeChanged();
    void colorsChanged();
    void miniDismissedChanged();

private:
    bool m_echoes = true;
    bool m_roundCover = false;
    bool m_panelDocked = false;
    qreal m_panelBottomGap = -1;
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
    QColor m_customColor2 = QColor(0xe0, 0x40, 0xfb);
    QColor m_coverColor;
    QList<QColor> m_coverColors;   // the main ones, the most telling first
    QUrl m_coverUrl;
};

/* Draws what the tap heard, in step with the playing time */
class AudioVisualizer : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(int style READ style WRITE setStyle NOTIFY styleChanged FINAL)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged FINAL)
    Q_PROPERTY(QColor color2 READ color2 WRITE setColor2 NOTIFY colorChanged FINAL)
    // Colours along the drawing (left to right, or from the top of the
    // ring both ways); fewer than two: color and color2 alone
    Q_PROPERTY(QVariantList gradient READ gradient WRITE setGradient NOTIFY colorChanged FINAL)
    // The cover image: the ring goes around it
    Q_PROPERTY(QQuickItem *cover READ cover WRITE setCover NOTIFY coverChanged FINAL)
    Q_PROPERTY(qreal level READ level NOTIFY levelChanged FINAL)
    // Use the whole item, as in the mini window, instead of keeping to the
    // edges of the music player
    Q_PROPERTY(bool fill READ fill WRITE setFill NOTIFY fillChanged FINAL)
    // The bottom corners of what holds it are rounded so much: what would
    // stick out is drawn on their curve
    Q_PROPERTY(qreal bottomRadius READ bottomRadius WRITE setBottomRadius NOTIFY bottomRadiusChanged FINAL)

public:
    explicit AudioVisualizer(QQuickItem *parent = nullptr);

    int style() const { return m_style; }
    void setStyle(int style);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);
    QColor color2() const { return m_color2; }
    void setColor2(const QColor &color);
    QVariantList gradient() const;
    void setGradient(const QVariantList &gradient);
    QQuickItem *cover() const { return m_cover; }
    void setCover(QQuickItem *cover);
    qreal level() const { return m_level; }
    bool fill() const { return m_fill; }
    void setFill(bool fill);
    qreal bottomRadius() const { return m_bottomRadius; }
    void setBottomRadius(qreal radius);

signals:
    void bottomRadiusChanged();
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
    QVector<QColor> m_stops;
    /* The colour at t (0..1) along the gradient, and its paler one */
    QColor tint(qreal t) const;
    QColor paleTint(qreal t) const;
    QPointer<QQuickItem> m_cover;
    QRectF m_coverRect;
    qreal m_level = 0;
    bool m_fill = false;
    qreal m_bottomRadius = 0;

    QVector<float> m_bands;
    QVector<float> m_peaks;
    QVector<float> m_echo1;
    QVector<float> m_echo2;
    QVector<float> m_wave;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
    QMetaObject::Connection m_frameConnection;
    /* Draws when the frames stop coming (after a power change the window
     * went on drawing only once a second, for the clock) */
    QTimer *m_kick = nullptr;
    qint64 m_lastFrameMs = 0;
    bool m_kickNoted = false;
    int m_kicks = 0;   /* in a row, without a frame */
};

}

#endif
