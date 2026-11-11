/*****************************************************************************
 * link_resolver.cpp: web pages to playable streams, through yt-dlp
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

#include "link_resolver.hpp"
#include "media_tools.hpp"
#include "components.hpp"

#include <vlc_common.h>
#include <vlc_interface.h> /* EXTENSIONS_MEDIA */

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QTimer>

namespace terrikon {

bool isWebPage(const QUrl &url)
{
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("http") && scheme != QLatin1String("https"))
        return false;
    const QString suffix = QFileInfo(url.path()).suffix().toLower();
    if (suffix.isEmpty())
        return true;
    /* a stream or a playlist VLC opens itself */
    static const QStringList direct = [] {
        QStringList list;
        for (const QString &pattern : QString::fromLatin1(EXTENSIONS_MEDIA ";" EXTENSIONS_PLAYLIST)
                                          .split(QLatin1Char(';'), Qt::SkipEmptyParts))
            list << pattern.mid(2).toLower(); /* "*.mkv" */
        list << QStringLiteral("m3u8") << QStringLiteral("mpd");
        return list;
    }();
    return !direct.contains(suffix);
}

static QStringList headerOptions(const QJsonObject &headers)
{
    QStringList options;
    const QString agent = headers.value(QStringLiteral("User-Agent")).toString();
    const QString referer = headers.value(QStringLiteral("Referer")).toString();
    if (!agent.isEmpty())
        options << QStringLiteral(":http-user-agent=") + agent;
    if (!referer.isEmpty())
        options << QStringLiteral(":http-referrer=") + referer;
    return options;
}

static ResolvedLink parse(const QByteArray &json)
{
    ResolvedLink link;
    const QJsonObject info = QJsonDocument::fromJson(json).object();
    if (info.isEmpty())
    {
        link.error = QStringLiteral("yt-dlp gave nothing to play.");
        return link;
    }
    link.title = info.value(QStringLiteral("title")).toString();

    /* A video and an audio stream to put together, or one with both */
    const QJsonArray formats = info.value(QStringLiteral("requested_formats")).toArray();
    if (formats.size() >= 2)
    {
        QJsonObject video, audio;
        for (const QJsonValue &value : formats)
        {
            const QJsonObject f = value.toObject();
            if (f.value(QStringLiteral("vcodec")).toString() != QLatin1String("none") && video.isEmpty())
                video = f;
            else if (audio.isEmpty())
                audio = f;
        }
        if (video.isEmpty())
            video = formats.at(0).toObject();
        link.mrl = video.value(QStringLiteral("url")).toString();
        link.options = headerOptions(video.value(QStringLiteral("http_headers")).toObject());
        const QString audioUrl = audio.value(QStringLiteral("url")).toString();
        if (!audioUrl.isEmpty())
            link.options << QStringLiteral(":input-slave=") + audioUrl;
    }
    else
    {
        link.mrl = info.value(QStringLiteral("url")).toString();
        link.options = headerOptions(info.value(QStringLiteral("http_headers")).toObject());
    }
    if (link.mrl.isEmpty())
        link.error = QStringLiteral("yt-dlp found no stream.");
    return link;
}

void resolveLink(QObject *context, const QUrl &url,
                 const std::function<void(const ResolvedLink &)> &done)
{
    const QString ytdlp = findTool(QStringLiteral("yt-dlp"));
    if (ytdlp.isEmpty())
    {
        ResolvedLink link;
        link.error = QStringLiteral("yt-dlp was not found.");
        QTimer::singleShot(0, context, [done, link] { done(link); });
        if (Components *components = Components::instance())
            QTimer::singleShot(0, components, [components] {
                components->offer(QStringLiteral("yt-dlp"), qtr("Opening links to video sites"));
            });
        return;
    }

    auto process = new QProcess(context);
    QPointer<QProcess> guard(process);
    /* At most 1080p (what a laptop screen shows), video and audio apart
     * when the site keeps them apart; no playlist, only the item linked */
    process->start(ytdlp, { QStringLiteral("--no-playlist"), QStringLiteral("--no-warnings"),
                            QStringLiteral("-f"), QStringLiteral("bv*[height<=1080]+ba/b[height<=1080]/b"),
                            QStringLiteral("-j"), url.toString(QUrl::FullyEncoded) });

    auto finished = std::make_shared<bool>(false);
    auto finish = [process, done, finished](const ResolvedLink &link) {
        if (*finished)
            return;
        *finished = true;
        process->deleteLater();
        done(link);
    };
    QObject::connect(process, &QProcess::finished, context,
                     [process, finish](int code, QProcess::ExitStatus status) {
        ResolvedLink link;
        if (status != QProcess::NormalExit || code != 0)
        {
            link.error = QString::fromUtf8(process->readAllStandardError()).trimmed().section(QLatin1Char('\n'), -1);
            if (link.error.isEmpty())
                link.error = QStringLiteral("yt-dlp failed.");
        }
        else
            link = parse(process->readAllStandardOutput());
        finish(link);
    });
    QObject::connect(process, &QProcess::errorOccurred, context, [finish](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        ResolvedLink link;
        link.error = QStringLiteral("yt-dlp could not be started.");
        finish(link);
    });
    /* A site that does not answer: the link goes to VLC as it is */
    QTimer::singleShot(30000, process, [process, finish] {
        process->kill();
        ResolvedLink link;
        link.error = QStringLiteral("yt-dlp took too long.");
        finish(link);
    });
}

}
