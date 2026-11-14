/*****************************************************************************
 * diagnostics.cpp: what happened, for finding out why
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

#include "diagnostics.hpp"

#include "player/player_controller.hpp"

#include <vlc_common.h>
#include <vlc_interface.h>

#include <QAbstractButton>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

#ifdef _WIN32
# include <windows.h>
# include <dbghelp.h>
#endif

namespace terrikon {

/* Qt messages come from any thread, also while the interface closes: the
 * handler counts itself in before it reads the instance, and the destructor
 * waits for the count after it has cleared it */
static std::atomic<Diagnostics *> s_instance{nullptr};
static std::atomic<int> s_handlersRunning{0};
static QtMessageHandler s_previousHandler = nullptr;

struct Diagnostics::Private
{
    qt_intf_t *intf = nullptr;
    QString folder;
    std::mutex fileLock;
    QFile file;

    /* the watchdog */
    std::atomic<qint64> heartbeat{0};
    std::atomic<bool> stopping{false};
    std::mutex wakeLock;
    std::condition_variable wake;
    std::thread watchdog;
    QTimer beat;

    void write(const QString &line)
    {
        const QByteArray bytes = (QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz "))
                                  + line + QLatin1Char('\n')).toUtf8();
        std::lock_guard<std::mutex> lock(fileLock);
        if (file.isOpen())
        {
            file.write(bytes);
            file.flush();
        }
    }
};

static qint64 nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type != QtDebugMsg && type != QtInfoMsg)
    {
        ++s_handlersRunning;
        if (Diagnostics *instance = s_instance.load())
        {
            static const char *const names[] = { "debug", "warning", "critical", "fatal", "info" };
            instance->note(QStringLiteral("qt %1: %2").arg(QLatin1String(names[qBound(0, int(type), 4)]), message));
        }
        --s_handlersRunning;
    }
    if (s_previousHandler)
        s_previousHandler(type, context, message);
}

#ifdef _WIN32
/* A dump of the process as it is, from another thread: where the
 * interface thread is stuck */
static QString writeDump(const QString &reason)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                      + QStringLiteral("/terrikon/crashes");
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/hang-")
                       + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))
                       + QStringLiteral(".dmp");
    HANDLE file = CreateFileW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()),
                              GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return {};
    HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll");
    using Fn = BOOL (WINAPI *)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                               PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    Fn dump = dbghelp ? reinterpret_cast<Fn>(reinterpret_cast<void (*)(void)>(GetProcAddress(dbghelp, "MiniDumpWriteDump"))) : nullptr;
    const bool ok = dump && dump(GetCurrentProcess(), GetCurrentProcessId(), file,
                                 MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory),
                                 nullptr, nullptr, nullptr);
    CloseHandle(file);
    (void) reason;
    return ok ? path : QString();
}
#endif

Diagnostics::Diagnostics(qt_intf_t *intf, QObject *parent)
    : QObject(parent), d(new Private)
{
    d->intf = intf;
    d->folder = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
              + QStringLiteral("/terrikon/logs");
    QDir dir(d->folder);
    dir.mkpath(QStringLiteral("."));
    /* the last ten runs */
    const QStringList old = dir.entryList({ QStringLiteral("*.log") }, QDir::Files, QDir::Name | QDir::Reversed);
    for (int i = 9; i < old.size(); ++i)
        dir.remove(old[i]);
    d->file.setFileName(dir.filePath(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"))
                                     + QStringLiteral(".log")));
    d->file.open(QIODevice::WriteOnly | QIODevice::Text);
    d->write(QStringLiteral("Terrikon %1 started").arg(QLatin1String(VERSION)));
}

Diagnostics::~Diagnostics()
{
    d->stopping = true;
    d->wake.notify_all();
    if (d->watchdog.joinable())
        d->watchdog.join();
    d->write(QStringLiteral("closed"));
    Diagnostics *self = this;
    if (s_instance.compare_exchange_strong(self, nullptr))
        while (s_handlersRunning.load() > 0)
            std::this_thread::yield();
    delete d;
}

Diagnostics *Diagnostics::instance()
{
    return s_instance;
}

QString Diagnostics::folder() const
{
    return d->folder;
}

void Diagnostics::note(const QString &what)
{
    d->write(what);
}

/* what a click landed on: the topmost item with an id or a text */
static QString describeAt(QQuickWindow *window, const QPointF &point)
{
    QString found;
    std::function<bool(QQuickItem *)> walk = [&](QQuickItem *item) -> bool {
        if (!item->isVisible() || item->opacity() <= 0.)
            return false;
        /* topmost first: higher z, then the later declared */
        QList<QQuickItem *> children = item->childItems();
        std::reverse(children.begin(), children.end());
        std::stable_sort(children.begin(), children.end(),
                         [](QQuickItem *a, QQuickItem *b) { return a->z() > b->z(); });
        for (QQuickItem *child : children)
            if (walk(child))
                return true;
        if (!item->contains(item->mapFromScene(point)))
            return false;
        /* layers over the whole window say nothing of what was clicked */
        if (item->width() >= window->width() * 0.95 && item->height() >= window->height() * 0.8)
            return false;
        QString id;
        if (QQmlContext *ctx = qmlContext(item))
            id = ctx->nameForObject(item);
        QString text;
        for (const char *name : { "text", "description", "title" })
        {
            const QVariant v = item->property(name);
            if (v.isValid() && v.canConvert<QString>() && !v.toString().isEmpty())
            {
                text = v.toString().left(60);
                break;
            }
        }
        if (id.isEmpty() && text.isEmpty())
            return false;
        QString type = QString::fromLatin1(item->metaObject()->className());
        type.remove(QRegularExpression(QStringLiteral("_QMLTYPE_\\d+|_QML_\\d+")));
        found = QStringLiteral("%1 %2 \"%3\"").arg(type, id, text);
        return true;
    };
    walk(window->contentItem());
    return found;
}

bool Diagnostics::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (auto *window = qobject_cast<QQuickWindow *>(watched))
        {
            const QString what = describeAt(window, mouse->position());
            d->write(QStringLiteral("click %1 at %2,%3 on %4")
                         .arg(mouse->button() == Qt::RightButton ? QStringLiteral("right") : QStringLiteral("left"))
                         .arg(qRound(mouse->position().x())).arg(qRound(mouse->position().y()))
                         .arg(what.isEmpty() ? QStringLiteral("(nothing named)") : what));
        }
        else if (auto *button = qobject_cast<QAbstractButton *>(watched))
            d->write(QStringLiteral("click button \"%1\" in \"%2\"")
                         .arg(button->text(), button->window()->windowTitle()));
    }
    else if (event->type() == QEvent::KeyPress && qobject_cast<QWindow *>(watched))
    {
        auto *key = static_cast<QKeyEvent *>(event);
        if (!key->isAutoRepeat())
            d->write(QStringLiteral("key %1").arg(QKeySequence(key->keyCombination()).toString()));
    }
    return QObject::eventFilter(watched, event);
}

Diagnostics *Diagnostics::install(qt_intf_t *intf, QGuiApplication &app)
{
    auto *self = new Diagnostics(intf, &app);
    s_instance = self;
    s_previousHandler = qInstallMessageHandler(messageHandler);
    app.installEventFilter(self);
    Private *d = self->d;

    /* the player */
    if (PlayerController *player = intf->p_mainPlayerController)
    {
        connect(player, &PlayerController::playingStateChanged, self, [d, player](PlayerController::PlayingState state) {
            static const char *const states[] = { "stopped", "started", "playing", "paused", "stopping" };
            const int i = int(state);
            d->write(QStringLiteral("player %1 at %2 s").arg(QLatin1String(i >= 0 && i < 5 ? states[i] : "?"))
                         .arg(player->getTime().toSeconds()));
        });
        connect(player, &PlayerController::inputChanged, self, [d, player](bool hasInput) {
            if (hasInput)
                d->write(QStringLiteral("media %1").arg(player->getUrl().toDisplayString()));
        });
    }

    /* the watchdog: the interface thread beats every half second */
    d->heartbeat = nowMs();
    d->beat.setInterval(500);
    connect(&d->beat, &QTimer::timeout, self, [d] { d->heartbeat = nowMs(); });
    d->beat.start();
    d->watchdog = std::thread([d] {
        bool frozen = false;
        qint64 since = 0;
        for (;;)
        {
            {
                std::unique_lock<std::mutex> lock(d->wakeLock);
                d->wake.wait_for(lock, std::chrono::seconds(1));
            }
            if (d->stopping)
                return;
            const qint64 late = nowMs() - d->heartbeat;
            if (!frozen && late > 4000)
            {
                frozen = true;
                since = d->heartbeat;
                QString dump;
#ifdef _WIN32
                dump = writeDump(QStringLiteral("hang"));
#endif
                d->write(QStringLiteral("NOT RESPONDING for %1 s%2").arg(late / 1000)
                             .arg(dump.isEmpty() ? QString() : QStringLiteral(", dump ") + dump));
            }
            else if (frozen && late < 1000)
            {
                frozen = false;
                d->write(QStringLiteral("responding again after %1 s").arg((nowMs() - since) / 1000));
            }
        }
    });
    return self;
}

}
