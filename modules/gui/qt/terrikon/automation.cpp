/*****************************************************************************
 * automation.cpp: driving the interface from scripts
 *****************************************************************************
 * Copyright (C) 2026 Terrikon contributors
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

#include "automation.hpp"

#include <vlc_common.h>
#include <vlc_network.h>
#include <vlc_player.h>
#include <vlc_input_item.h>
#include <vlc_interface.h>
#include <vlc_vout.h>

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMetaProperty>
#include <QMouseEvent>
#include <QPointer>
#include <QProgressBar>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace terrikon {

struct Automation::Private
{
    qt_intf_t *intf = nullptr;
    int listenFd = -1;
    std::atomic<int> clientFd{-1};
    std::atomic<bool> stopping{false};
    std::thread thread;
    /* the items of the last listing, for "ref:N" */
    QVector<QPointer<QObject>> refs;
    int window = -1;
};

namespace {

QString typeName(const QObject *o)
{
    QString n = QString::fromLatin1(o->metaObject()->className());
    static const QRegularExpression qmlSuffix(QStringLiteral("_QMLTYPE_\\d+|_QML_\\d+"));
    n.remove(qmlSuffix);
    if (n.startsWith(QLatin1String("QQuick")))
        n = n.mid(6);
    return n;
}

QString qmlId(QObject *o)
{
    if (QQmlContext *ctx = qmlContext(o))
        return ctx->nameForObject(o);
    return {};
}

QString textOf(const QObject *o)
{
    for (const char *name : { "text", "title", "label", "placeholderText" })
    {
        const QVariant v = o->property(name);
        if (v.isValid() && v.canConvert<QString>())
        {
            const QString s = v.toString();
            if (!s.isEmpty())
                return s;
        }
    }
    return {};
}

bool shownInScene(QQuickItem *item)
{
    for (QQuickItem *i = item; i; i = i->parentItem())
        if (!i->isVisible() || i->opacity() <= 0.)
            return false;
    return true;
}

QList<QQuickWindow *> quickWindows()
{
    QList<QQuickWindow *> list;
    for (QWindow *w : QGuiApplication::topLevelWindows())
        if (auto *q = qobject_cast<QQuickWindow *>(w); q && q->isVisible())
            list << q;
    return list;
}

QJsonValue toJson(const QVariant &v)
{
    if (v.metaType().flags() & QMetaType::PointerToQObject)
    {
        QObject *o = v.value<QObject *>();
        if (!o)
            return QJsonValue::Null;
        return QStringLiteral("<%1 %2>").arg(typeName(o), qmlId(o));
    }
    if (v.canConvert<QJSValue>())
    {
        const QJSValue js = v.value<QJSValue>();
        if (!js.isObject())
            return QJsonValue::fromVariant(js.toVariant());
    }
    const QJsonValue j = QJsonValue::fromVariant(v);
    if (j.isNull() && v.isValid() && v.canConvert<QString>())
        return v.toString();
    return j;
}

QString keyText(int key)
{
    return QKeySequence(key).toString();
}

} // namespace

static QJsonObject error(const QString &message)
{
    return QJsonObject{ { QStringLiteral("error"), message } };
}

/* Everything below runs on the interface thread */
class Commands
{
public:
    explicit Commands(Automation::Private *d) : d(d) {}

    QJsonObject run(const QString &line)
    {
        const int space = line.indexOf(QLatin1Char(' '));
        const QString cmd = (space < 0 ? line : line.left(space)).trimmed().toLower();
        const QString rest = space < 0 ? QString() : line.mid(space + 1).trimmed();
        const QStringList args = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);

        if (cmd == QLatin1String("help"))
            return help();
        if (cmd == QLatin1String("windows"))
            return windows();
        if (cmd == QLatin1String("window"))
        {
            d->window = args.value(0, QStringLiteral("-1")).toInt();
            return { { QStringLiteral("window"), d->window } };
        }
        if (cmd == QLatin1String("items"))
            return items(rest, false);
        if (cmd == QLatin1String("all"))
            return items(rest, true);
        if (cmd == QLatin1String("at"))
            return at(args);
        if (cmd == QLatin1String("widgets"))
            return widgets(rest);
        if (cmd == QLatin1String("move") || cmd == QLatin1String("hover"))
            return mouse(args, QStringLiteral("move"));
        if (cmd == QLatin1String("click") || cmd == QLatin1String("dblclick")
            || cmd == QLatin1String("rclick") || cmd == QLatin1String("press")
            || cmd == QLatin1String("release") || cmd == QLatin1String("mclick"))
            return mouse(args, cmd);
        if (cmd == QLatin1String("leave"))
            return leave();
        if (cmd == QLatin1String("wheel"))
            return wheel(args);
        if (cmd == QLatin1String("key"))
            return key(args);
        if (cmd == QLatin1String("type"))
            return type(rest);
        if (cmd == QLatin1String("get"))
            return property(args, false);
        if (cmd == QLatin1String("set"))
            return property(args, true);
        if (cmd == QLatin1String("props"))
            return props(args);
        if (cmd == QLatin1String("call"))
            return call(args);
        if (cmd == QLatin1String("eval"))
            return eval(rest);
        if (cmd == QLatin1String("grab"))
            return grab(args);
        if (cmd == QLatin1String("player"))
            return player();
        return error(QStringLiteral("unknown command, try help"));
    }

private:
    Automation::Private *d;

    static QJsonObject help()
    {
        return { { QStringLiteral("commands"), QJsonArray{
            "windows: the windows open",
            "window N: the window the next commands act on (-1: the main one)",
            "items [filter]: shown items having an id or a text, numbered (ref:N)",
            "all [filter]: every shown item",
            "at X Y: the items under a point, topmost first",
            "widgets [filter]: buttons, labels, fields of the dialogs, numbered",
            "hover X Y | hover ref:N: move the pointer there",
            "leave: the pointer leaves the window",
            "click / dblclick / rclick / mclick / press / release X Y | ref:N",
            "wheel X Y DY: scroll (DY in steps, negative: down)",
            "key NAME [count]: a key or chord, as Return, Space, Ctrl+F, Escape",
            "type TEXT: type text into the focused item",
            "get ref:N|id PROPERTY; set ref:N|id PROPERTY JSON",
            "props ref:N|id: the item's properties",
            "call ref:N|id METHOD: calls a method or signal without arguments (clicked, open...)",
            "eval JS: a QML expression in the main window (its ids, MainCtx, Player...)",
            "grab PATH [scale]: the window as a PNG",
            "player: what the main player is doing" } } };
    }

    QQuickWindow *targetWindow()
    {
        const QList<QQuickWindow *> list = quickWindows();
        if (d->window >= 0 && d->window < list.size())
            return list[d->window];
        QQuickWindow *best = nullptr;
        for (QQuickWindow *w : list)
            if (!best || w->width() * w->height() > best->width() * best->height())
                best = w;
        return best;
    }

    QJsonObject windows()
    {
        QJsonArray quick, dialogs;
        int n = 0;
        for (QQuickWindow *w : quickWindows())
            quick.append(QJsonObject{ { QStringLiteral("n"), n++ },
                                      { QStringLiteral("title"), w->title() },
                                      { QStringLiteral("rect"), QJsonArray{ w->x(), w->y(), w->width(), w->height() } },
                                      { QStringLiteral("active"), w->isActive() } });
        for (QWidget *w : QApplication::topLevelWidgets())
            if (w->isVisible() && !w->windowTitle().isNull() && !qobject_cast<QMenu *>(w))
                dialogs.append(QJsonObject{ { QStringLiteral("title"), w->windowTitle() },
                                            { QStringLiteral("type"), typeName(w) },
                                            { QStringLiteral("rect"), QJsonArray{ w->x(), w->y(), w->width(), w->height() } } });
        return { { QStringLiteral("quick"), quick }, { QStringLiteral("widgets"), dialogs } };
    }

    QJsonObject describe(QQuickItem *item, int ref)
    {
        const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        QJsonObject o{ { QStringLiteral("ref"), ref },
                       { QStringLiteral("type"), typeName(item) },
                       { QStringLiteral("rect"), QJsonArray{ qRound(r.x()), qRound(r.y()), qRound(r.width()), qRound(r.height()) } } };
        if (const QString id = qmlId(item); !id.isEmpty())
            o[QStringLiteral("id")] = id;
        if (const QString t = textOf(item); !t.isEmpty())
            o[QStringLiteral("text")] = t.left(120);
        if (!item->isEnabled())
            o[QStringLiteral("disabled")] = true;
        if (item->hasActiveFocus())
            o[QStringLiteral("focus")] = true;
        for (const char *flag : { "hovered", "checked", "pressed", "highlighted", "selected" })
        {
            const QVariant v = item->property(flag);
            if (v.typeId() == QMetaType::Bool && v.toBool())
                o[QLatin1String(flag)] = true;
        }
        return o;
    }

    QJsonObject items(const QString &filter, bool everything)
    {
        QQuickWindow *w = targetWindow();
        if (!w)
            return error(QStringLiteral("no window"));
        d->refs.clear();
        QJsonArray list;
        const QRectF view(0, 0, w->width(), w->height());
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
            if (!item->isVisible() || item->opacity() <= 0.)
                return;
            const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
            const bool onScreen = r.width() > 0 && r.height() > 0 && r.intersects(view);
            if (onScreen)
            {
                const QString id = qmlId(item);
                const QString text = textOf(item);
                const bool wanted = everything || !id.isEmpty() || !text.isEmpty();
                const bool matches = filter.isEmpty()
                    || id.contains(filter, Qt::CaseInsensitive)
                    || text.contains(filter, Qt::CaseInsensitive)
                    || typeName(item).contains(filter, Qt::CaseInsensitive);
                if (wanted && matches)
                {
                    d->refs.append(item);
                    list.append(describe(item, d->refs.size() - 1));
                }
            }
            for (QQuickItem *child : item->childItems())
                walk(child);
        };
        walk(w->contentItem());
        return { { QStringLiteral("items"), list } };
    }

    QJsonObject at(const QStringList &args)
    {
        QQuickWindow *w = targetWindow();
        if (!w || args.size() < 2)
            return error(QStringLiteral("at X Y"));
        const QPointF p(args[0].toDouble(), args[1].toDouble());
        d->refs.clear();
        QJsonArray list;
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
            if (!item->isVisible() || item->opacity() <= 0.)
                return;
            QList<QQuickItem *> children = item->childItems();
            std::reverse(children.begin(), children.end());
            std::stable_sort(children.begin(), children.end(),
                             [](QQuickItem *a, QQuickItem *b) { return a->z() > b->z(); });
            for (auto it = children.begin(); it != children.end(); ++it)
                walk(*it);
            if (item->contains(item->mapFromScene(p)))
            {
                d->refs.append(item);
                list.append(describe(item, d->refs.size() - 1));
            }
        };
        walk(w->contentItem());
        return { { QStringLiteral("items"), list } };
    }

    QJsonObject widgets(const QString &filter)
    {
        d->refs.clear();
        QJsonArray list;
        for (QWidget *top : QApplication::topLevelWidgets())
        {
            if (!top->isVisible())
                continue;
            for (QWidget *w : top->findChildren<QWidget *>())
            {
                if (!w->isVisible())
                    continue;
                QString text;
                QJsonObject o;
                if (auto *b = qobject_cast<QAbstractButton *>(w))
                {
                    text = b->text();
                    if (b->isCheckable())
                        o[QStringLiteral("checked")] = b->isChecked();
                }
                else if (auto *l = qobject_cast<QLabel *>(w))
                    text = l->text();
                else if (auto *e = qobject_cast<QLineEdit *>(w))
                    text = e->text();
                else if (auto *c = qobject_cast<QComboBox *>(w))
                    text = c->currentText();
                else if (auto *p = qobject_cast<QProgressBar *>(w))
                {
                    o[QStringLiteral("value")] = p->value();
                    o[QStringLiteral("max")] = p->maximum();
                    text = p->format();
                }
                else if (auto *s = qobject_cast<QAbstractSlider *>(w))
                    o[QStringLiteral("value")] = s->value();
                else
                    continue;
                if (!filter.isEmpty() && !text.contains(filter, Qt::CaseInsensitive)
                    && !top->windowTitle().contains(filter, Qt::CaseInsensitive))
                    continue;
                const QPoint g = w->mapToGlobal(QPoint(0, 0));
                d->refs.append(w);
                o[QStringLiteral("ref")] = d->refs.size() - 1;
                o[QStringLiteral("type")] = typeName(w);
                o[QStringLiteral("window")] = top->windowTitle();
                o[QStringLiteral("text")] = text.left(160);
                o[QStringLiteral("rect")] = QJsonArray{ g.x(), g.y(), w->width(), w->height() };
                if (!w->isEnabled())
                    o[QStringLiteral("disabled")] = true;
                list.append(o);
            }
        }
        return { { QStringLiteral("widgets"), list } };
    }

    /* "ref:N" or a QML id; the shown item first */
    QObject *object(const QString &name)
    {
        if (name.startsWith(QLatin1String("ref:")))
        {
            const int n = name.mid(4).toInt();
            return n >= 0 && n < d->refs.size() ? d->refs[n].data() : nullptr;
        }
        QQuickWindow *w = targetWindow();
        if (!w)
            return nullptr;
        QQuickItem *hidden = nullptr;
        QQuickItem *found = nullptr;
        std::function<void(QQuickItem *)> walk = [&](QQuickItem *item) {
            if (found)
                return;
            if (qmlId(item) == name)
            {
                if (shownInScene(item))
                {
                    found = item;
                    return;
                }
                if (!hidden)
                    hidden = item;
            }
            for (QQuickItem *child : item->childItems())
                walk(child);
        };
        walk(w->contentItem());
        return found ? found : hidden;
    }

    bool point(const QStringList &args, QQuickWindow *&w, QPointF &p, int &used)
    {
        w = targetWindow();
        if (!w || args.isEmpty())
            return false;
        if (args[0].startsWith(QLatin1String("ref:")))
        {
            auto *item = qobject_cast<QQuickItem *>(object(args[0]));
            if (!item)
                return false;
            if (item->window())
                w = item->window();
            p = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
            used = 1;
            return true;
        }
        if (args.size() < 2)
            return false;
        p = QPointF(args[0].toDouble(), args[1].toDouble());
        used = 2;
        return true;
    }

    static void send(QWindow *w, QEvent::Type type, QPointF p, Qt::MouseButton button, Qt::MouseButtons buttons)
    {
        QMouseEvent ev(type, p, w->mapToGlobal(p), button, buttons, QGuiApplication::keyboardModifiers());
        QCoreApplication::sendEvent(w, &ev);
    }

    QJsonObject mouse(const QStringList &args, const QString &what)
    {
        /* widgets: click them for real */
        if (!args.isEmpty() && args[0].startsWith(QLatin1String("ref:")))
            if (auto *b = qobject_cast<QAbstractButton *>(object(args[0])))
            {
                b->click();
                return { { QStringLiteral("clicked"), b->text() } };
            }
        QQuickWindow *w;
        QPointF p;
        int used = 0;
        if (!point(args, w, p, used))
            return error(QStringLiteral("X Y or ref:N"));
        send(w, QEvent::MouseMove, p, Qt::NoButton, Qt::NoButton);
        Qt::MouseButton button = what == QLatin1String("rclick") ? Qt::RightButton
                               : what == QLatin1String("mclick") ? Qt::MiddleButton : Qt::LeftButton;
        if (what == QLatin1String("press"))
            send(w, QEvent::MouseButtonPress, p, button, button);
        else if (what == QLatin1String("release"))
            send(w, QEvent::MouseButtonRelease, p, button, Qt::NoButton);
        else if (what != QLatin1String("move"))
        {
            send(w, QEvent::MouseButtonPress, p, button, button);
            send(w, QEvent::MouseButtonRelease, p, button, Qt::NoButton);
            if (what == QLatin1String("dblclick"))
            {
                send(w, QEvent::MouseButtonPress, p, button, button);
                send(w, QEvent::MouseButtonDblClick, p, button, button);
                send(w, QEvent::MouseButtonRelease, p, button, Qt::NoButton);
            }
        }
        return { { QStringLiteral("at"), QJsonArray{ qRound(p.x()), qRound(p.y()) } } };
    }

    QJsonObject leave()
    {
        QQuickWindow *w = targetWindow();
        if (!w)
            return error(QStringLiteral("no window"));
        /* far outside, then the window is left */
        send(w, QEvent::MouseMove, QPointF(-100, -100), Qt::NoButton, Qt::NoButton);
        QEvent ev(QEvent::Leave);
        QCoreApplication::sendEvent(w, &ev);
        return { { QStringLiteral("left"), true } };
    }

    QJsonObject wheel(const QStringList &args)
    {
        QQuickWindow *w;
        QPointF p;
        int used = 0;
        if (!point(args, w, p, used))
            return error(QStringLiteral("wheel X Y DY"));
        const int steps = args.value(used, QStringLiteral("-1")).toInt();
        send(w, QEvent::MouseMove, p, Qt::NoButton, Qt::NoButton);
        QWheelEvent ev(p, w->mapToGlobal(p), QPoint(), QPoint(0, steps * 120), Qt::NoButton,
                       QGuiApplication::keyboardModifiers(), Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(w, &ev);
        return { { QStringLiteral("scrolled"), steps } };
    }

    QJsonObject key(const QStringList &args)
    {
        if (args.isEmpty())
            return error(QStringLiteral("key NAME"));
        const QKeySequence seq = QKeySequence::fromString(args[0], QKeySequence::PortableText);
        if (seq.isEmpty())
            return error(QStringLiteral("unknown key"));
        const QKeyCombination combo = seq[0];
        const int count = qMax(1, args.value(1, QStringLiteral("1")).toInt());
        QObject *target = QGuiApplication::focusObject();
        QWindow *window = QGuiApplication::focusWindow();
        if (!window)
            window = targetWindow();
        if (!target)
            target = window;
        if (!target)
            return error(QStringLiteral("nothing has the focus"));
        for (int i = 0; i < count; ++i)
        {
            QKeyEvent press(QEvent::KeyPress, combo.key(), combo.keyboardModifiers());
            QCoreApplication::sendEvent(window ? static_cast<QObject *>(window) : target, &press);
            QKeyEvent release(QEvent::KeyRelease, combo.key(), combo.keyboardModifiers());
            QCoreApplication::sendEvent(window ? static_cast<QObject *>(window) : target, &release);
        }
        return { { QStringLiteral("key"), keyText(combo.toCombined()) } };
    }

    QJsonObject type(const QString &text)
    {
        QWindow *window = QGuiApplication::focusWindow();
        if (!window)
            window = targetWindow();
        if (!window)
            return error(QStringLiteral("no window"));
        for (const QChar c : text)
        {
            QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(window, &press);
            QKeyEvent release(QEvent::KeyRelease, 0, Qt::NoModifier, QString(c));
            QCoreApplication::sendEvent(window, &release);
        }
        return { { QStringLiteral("typed"), text.size() } };
    }

    QJsonObject property(const QStringList &args, bool write)
    {
        if (args.size() < (write ? 3 : 2))
            return error(write ? QStringLiteral("set ref:N|id PROPERTY JSON") : QStringLiteral("get ref:N|id PROPERTY"));
        QObject *o = object(args[0]);
        if (!o)
            return error(QStringLiteral("no such item"));
        const QByteArray name = args[1].toUtf8();
        if (write)
        {
            const QString json = args.mid(2).join(QLatin1Char(' '));
            const QJsonDocument doc = QJsonDocument::fromJson("[" + json.toUtf8() + "]");
            const QVariant value = doc.isArray() && !doc.array().isEmpty()
                ? doc.array().first().toVariant() : QVariant(json);
            if (!o->setProperty(name.constData(), value))
                return error(QStringLiteral("not set"));
        }
        return { { QString::fromUtf8(name), toJson(o->property(name.constData())) } };
    }

    QJsonObject props(const QStringList &args)
    {
        QObject *o = object(args.value(0));
        if (!o)
            return error(QStringLiteral("no such item"));
        QJsonObject list;
        const QMetaObject *mo = o->metaObject();
        for (int i = 0; i < mo->propertyCount(); ++i)
        {
            const QMetaProperty p = mo->property(i);
            if (i < QQuickItem::staticMetaObject.propertyCount() && qobject_cast<QQuickItem *>(o))
                continue; /* the many of every item: get them by name */
            list[QString::fromLatin1(p.name())] = toJson(p.read(o));
        }
        return { { QStringLiteral("type"), typeName(o) }, { QStringLiteral("props"), list } };
    }

    QJsonObject call(const QStringList &args)
    {
        QObject *o = object(args.value(0));
        if (!o || args.size() < 2)
            return error(QStringLiteral("call ref:N|id METHOD"));
        if (!QMetaObject::invokeMethod(o, args[1].toUtf8().constData(), Qt::DirectConnection))
            return error(QStringLiteral("no such method"));
        return { { QStringLiteral("called"), args[1] } };
    }

    QJsonObject eval(const QString &js)
    {
        QQuickWindow *w = targetWindow();
        if (!w)
            return error(QStringLiteral("no window"));
        /* the root of the main window's QML: its ids and imports */
        QQuickItem *root = nullptr;
        for (QQuickItem *child : w->contentItem()->childItems())
            if (qmlContext(child)) { root = child; break; }
        if (!root)
            return error(QStringLiteral("no QML"));
        QQmlExpression expr(qmlContext(root), root, js);
        bool undefined = false;
        const QVariant v = expr.evaluate(&undefined);
        if (expr.hasError())
            return error(expr.error().toString());
        return { { QStringLiteral("value"), undefined ? QJsonValue(QJsonValue::Undefined) : toJson(v) } };
    }

    QJsonObject grab(const QStringList &args)
    {
        if (args.isEmpty())
            return error(QStringLiteral("grab PATH [scale]"));
        const double scale = args.value(1, QStringLiteral("1")).toDouble();
        QImage image;
        /* a dialog in front: that one */
        if (QWidget *active = QApplication::activeWindow(); active && active->isVisible()
            && !qobject_cast<QQuickWindow *>(active->windowHandle()))
            image = active->grab().toImage();
        else if (QQuickWindow *w = targetWindow())
            image = w->grabWindow();
        if (image.isNull())
            return error(QStringLiteral("nothing to grab"));
        if (scale > 0. && scale < 1.)
            image = image.scaled(image.size() * scale, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QString path = QDir::fromNativeSeparators(args[0]);
        if (!image.save(path, "PNG"))
            return error(QStringLiteral("cannot save"));
        return { { QStringLiteral("saved"), path }, { QStringLiteral("size"), QJsonArray{ image.width(), image.height() } } };
    }

    QJsonObject player()
    {
        vlc_player_t *p = d->intf->p_player;
        if (!p)
            return error(QStringLiteral("no player"));
        QJsonObject o;
        vlc_player_Lock(p);
        static const char *const states[] = { "stopped", "started", "playing", "paused", "stopping" };
        const int state = vlc_player_GetState(p);
        o[QStringLiteral("state")] = state >= 0 && state < 5 ? states[state] : "?";
        o[QStringLiteral("error")] = vlc_player_GetError(p) != VLC_PLAYER_ERROR_NONE;
        const vlc_tick_t time = vlc_player_GetTime(p), length = vlc_player_GetLength(p);
        o[QStringLiteral("time")] = time == VLC_TICK_INVALID ? -1. : secf_from_vlc_tick(time);
        o[QStringLiteral("length")] = length == VLC_TICK_INVALID ? -1. : secf_from_vlc_tick(length);
        o[QStringLiteral("rate")] = vlc_player_GetRate(p);
        o[QStringLiteral("seekable")] = vlc_player_CanSeek(p);
        o[QStringLiteral("volume")] = vlc_player_aout_GetVolume(p);
        o[QStringLiteral("muted")] = vlc_player_aout_IsMuted(p) == 1;
        o[QStringLiteral("videoTracks")] = (int) vlc_player_GetVideoTrackCount(p);
        o[QStringLiteral("audioTracks")] = (int) vlc_player_GetAudioTrackCount(p);
        size_t vouts = 0;
        if (vout_thread_t **list = vlc_player_vout_HoldAll(p, &vouts))
        {
            for (size_t i = 0; i < vouts; ++i)
                vout_Release(list[i]);
            free(list);
        }
        o[QStringLiteral("videoOutputs")] = (int) vouts;
        if (input_item_t *item = vlc_player_GetCurrentMedia(p))
        {
            char *title = input_item_GetTitleFbName(item);
            o[QStringLiteral("title")] = QString::fromUtf8(title ? title : "");
            free(title);
            char *uri = input_item_GetURI(item);
            o[QStringLiteral("uri")] = QString::fromUtf8(uri ? uri : "");
            free(uri);
        }
        vlc_player_Unlock(p);
        return o;
    }
};

Automation::Automation(qt_intf_t *intf, QObject *parent)
    : QObject(parent), d(new Private)
{
    d->intf = intf;
}

Automation::~Automation()
{
    d->stopping = true;
    if (d->listenFd >= 0)
        net_Close(d->listenFd);
    if (int fd = d->clientFd.exchange(-1); fd >= 0)
        net_Close(fd);
    if (d->thread.joinable())
        d->thread.join();
    delete d;
}

void Automation::install(qt_intf_t *intf, QGuiApplication &app)
{
    const QByteArray port = qgetenv("TERRIKON_AUTOMATION");
    if (port.isEmpty() || port.toInt() <= 0)
        return;

    int fd = vlc_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, false);
    if (fd < 0)
        return;
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port.toInt());
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* never from elsewhere */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(fd, 4) != 0)
    {
        msg_Warn(intf->intf, "automation: cannot listen on port %s", port.constData());
        net_Close(fd);
        return;
    }
    msg_Info(intf->intf, "automation on 127.0.0.1:%s", port.constData());

    auto *self = new Automation(intf, &app);
    Private *d = self->d;
    d->listenFd = fd;
    d->thread = std::thread([self, d] {
        for (;;)
        {
            int client = accept(d->listenFd, nullptr, nullptr);
            if (client < 0 || d->stopping)
                return;
            d->clientFd = client;
            QByteArray pending;
            char buf[4096];
            for (;;)
            {
                const int n = recv(client, buf, sizeof(buf), 0);
                if (n <= 0 || d->stopping)
                    break;
                pending.append(buf, n);
                int eol;
                while ((eol = pending.indexOf('\n')) >= 0)
                {
                    const QString line = QString::fromUtf8(pending.left(eol)).trimmed();
                    pending.remove(0, eol + 1);
                    if (line.isEmpty())
                        continue;

                    /* on the interface thread; given up when closing */
                    struct Call { std::mutex lock; std::condition_variable done; bool finished = false; QByteArray reply; };
                    auto call = std::make_shared<Call>();
                    QMetaObject::invokeMethod(self, [d, line, call] {
                        QJsonObject r;
                        try { r = Commands(d).run(line); }
                        catch (...) { r = error(QStringLiteral("failed")); }
                        std::lock_guard<std::mutex> lock(call->lock);
                        call->reply = QJsonDocument(r).toJson(QJsonDocument::Compact);
                        call->finished = true;
                        call->done.notify_one();
                    }, Qt::QueuedConnection);
                    std::unique_lock<std::mutex> lock(call->lock);
                    while (!call->finished && !d->stopping)
                        call->done.wait_for(lock, std::chrono::milliseconds(100));
                    if (d->stopping)
                        break;
                    QByteArray reply = call->reply + '\n';
                    send(client, reply.constData(), reply.size(), 0);
                }
                if (d->stopping)
                    break;
            }
            if (int fd = d->clientFd.exchange(-1); fd >= 0)
                net_Close(fd);
            if (d->stopping)
                return;
        }
    });
}

}
