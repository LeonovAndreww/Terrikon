/*****************************************************************************
 * lyrics_finder.cpp: Terrikon, lyrics for songs without them
 *****************************************************************************
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "lyrics_finder.hpp"
#include "media_tools.hpp"
#include "player/player_controller.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_stream.h>

#include <algorithm>
#include <cmath>
#include <map>

namespace terrikon {

namespace {

QString cacheFolder()
{
    char *dir = config_GetUserDir(VLC_CACHE_DIR);
    const QString path = dir ? QString::fromUtf8(dir) + QStringLiteral("/lyrics") : QString();
    free(dir);
    return path;
}

QString cacheFile(const QString &key)
{
    const QString folder = cacheFolder();
    if (folder.isEmpty() || key.isEmpty())
        return {};
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return folder + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".lrc");
}

/* What an address answers, up to 2 MiB; empty if it does not */
QByteArray fetch(vlc_object_t *obj, const QUrl &url, bool *reached)
{
    QByteArray body;
    stream_t *s = vlc_stream_NewURL(obj, qtu(url.toString(QUrl::FullyEncoded)));
    if (!s)
        return body;
    *reached = true;
    char buf[16384];
    for (;;)
    {
        const ssize_t n = vlc_stream_Read(s, buf, sizeof(buf));
        if (n <= 0 || body.size() > 2 * 1024 * 1024)
            break;
        body.append(buf, n);
    }
    vlc_stream_Delete(s);
    return body;
}

/* LRC with its times multiplied by factor (a version played faster) */
QString scaleLrc(const QString &lrc, double factor)
{
    static const QRegularExpression stamp(QStringLiteral("\\[(\\d+):(\\d+(?:[.:]\\d+)?)\\]"));
    QString out;
    qsizetype from = 0;
    for (auto it = stamp.globalMatch(lrc); it.hasNext();)
    {
        const auto m = it.next();
        out += lrc.mid(from, m.capturedStart() - from);
        QString sec = m.captured(2);
        sec.replace(QLatin1Char(':'), QLatin1Char('.'));
        const double t = (m.captured(1).toLongLong() * 60 + sec.toDouble()) * factor;
        const long long cs = std::llround(t * 100);
        out += QString::asprintf("[%02lld:%02lld.%02lld]", cs / 6000, (cs / 100) % 60, cs % 100);
        from = m.capturedEnd();
    }
    return out + lrc.mid(from);
}

QString lyricsOf(const QJsonObject &o)
{
    const QString synced = o.value(QStringLiteral("syncedLyrics")).toString();
    return synced.isEmpty() ? o.value(QStringLiteral("plainLyrics")).toString() : synced;
}

}

LyricsFinder::LyricsFinder(qt_intf_t *intf, QObject *parent)
    : QObject(parent), m_intf(intf)
{
    PlayerController *player = intf->p_mainPlayerController;
    auto *later = new QTimer(this);
    later->setSingleShot(true);
    later->setInterval(300);
    connect(later, &QTimer::timeout, this, &LyricsFinder::lookLocally);
    connect(player, &PlayerController::inputChanged, this, [this, later] {
        setState(Idle);
        m_lookedAt.clear(); // played again: looked at again
        later->start();
    });
    connect(player, &PlayerController::currentMetaChanged, later, qOverload<>(&QTimer::start));
}

void LyricsFinder::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}

LyricsFinder::Song LyricsFinder::current() const
{
    PlayerController *player = m_intf->p_mainPlayerController;
    Song song;
    song.url = player->getUrl().toString(QUrl::FullyEncoded);
    song.artist = player->getArtist().trimmed();
    song.title = player->getTitle().trimmed();
    song.album = player->getAlbum().trimmed();
    const double length = player->getLength().toSecf();
    song.seconds = length > 0 ? int(std::lround(length)) : 0;
    /* the song, wherever its file is; else the file */
    song.key = song.title.isEmpty() ? song.url
             : song.artist.toLower() + QLatin1Char('\n') + song.title.toLower();
    return song;
}

void LyricsFinder::apply(const QString &url, const QByteArray &raw)
{
    PlayerController *player = m_intf->p_mainPlayerController;
    if (raw.isEmpty() || player->getUrl().toString(QUrl::FullyEncoded) != url)
        return;
    if (!player->hasLyrics())
        player->setExternalLyrics(raw);
}

void LyricsFinder::lookLocally()
{
    PlayerController *player = m_intf->p_mainPlayerController;
    const Song song = current();
    if (song.url.isEmpty() || player->hasLyrics())
        return;
    const QString looked = song.url + QLatin1Char('\n') + song.key;
    if (looked == m_lookedAt)
        return;
    m_lookedAt = looked;

    const QUrl url(song.url);
    const QString sidecar = url.isLocalFile()
        ? QFileInfo(url.toLocalFile()).path() + QLatin1Char('/')
          + QFileInfo(url.toLocalFile()).completeBaseName() + QStringLiteral(".lrc")
        : QString();
    const QString kept = cacheFile(song.key);
    /* next to the file: maybe on a drive slow to answer */
    offThread(this, [sidecar, kept] {
        for (const QString &path : { sidecar, kept })
        {
            if (path.isEmpty())
                continue;
            QFile file(path);
            if (file.size() > 0 && file.size() < 2 * 1024 * 1024 && file.open(QIODevice::ReadOnly))
                return fromLrc(QString::fromUtf8(file.readAll()));
        }
        return QByteArray();
    }, [this, song](const QByteArray &raw) {
        apply(song.url, raw);
    });
}

void LyricsFinder::find()
{
    if (m_state == Searching)
        return;
    const Song song = current();
    if (song.url.isEmpty())
        return;
    if (song.title.isEmpty() && QFileInfo(QUrl(song.url).path()).completeBaseName().isEmpty())
    {
        setState(NotFound);
        return;
    }
    setState(Searching);

    vlc_object_t *obj = VLC_OBJECT(m_intf);
    const QString kept = cacheFile(song.key);
    struct Result { QString lrc; bool reached = false; };
    offThread(this, [obj, song, kept] {
        Result result;
        const QString base = QStringLiteral("https://lrclib.net/api/");

        /* What to ask for: the tags as they are, then the title cleaned of
         * what is in brackets ("(speed up)", "[Official Video]") and split
         * when it holds the artist too ("Artist - Title", as files from the
         * web often are) */
        static const QRegularExpression brackets(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
        static const QRegularExpression speedMark(
            QStringLiteral("speed|sped|nightcore|slowed|ускор|замедл"),
            QRegularExpression::CaseInsensitiveOption);
        const QString title = song.title.isEmpty()
            ? QFileInfo(QUrl(song.url).path()).completeBaseName() : song.title;
        /* a version played faster or slower: the same words, other times */
        const bool otherSpeed = speedMark.match(title).hasMatch();
        struct Ask { QString artist, title; };
        QList<Ask> asks;
        if (!song.title.isEmpty())
            asks << Ask{ song.artist, song.title };
        QString clean = title;
        clean.remove(brackets);
        clean = clean.trimmed();
        if (const qsizetype dash = clean.indexOf(QStringLiteral(" - ")); dash > 0)
            asks << Ask{ clean.left(dash).trimmed(), clean.mid(dash + 3).trimmed() };
        else if (clean != song.title || song.title.isEmpty())
            asks << Ask{ song.artist, clean };

        /* the exact song first: by its artist, title, album and length */
        if (!song.artist.isEmpty() && !song.title.isEmpty() && song.seconds > 0)
        {
            QUrl url(base + QStringLiteral("get"));
            QUrlQuery q;
            q.addQueryItem(QStringLiteral("artist_name"), song.artist);
            q.addQueryItem(QStringLiteral("track_name"), song.title);
            if (!song.album.isEmpty())
                q.addQueryItem(QStringLiteral("album_name"), song.album);
            q.addQueryItem(QStringLiteral("duration"), QString::number(song.seconds));
            url.setQuery(q);
            const QJsonDocument doc = QJsonDocument::fromJson(fetch(obj, url, &result.reached));
            if (doc.isObject())
                result.lrc = lyricsOf(doc.object());
        }

        /* else, for each way of asking, the nearest by length among those
         * of that name, with times before without */
        for (int i = 0; i < asks.size() * 2 && result.lrc.isEmpty(); ++i)
        {
            const Ask &ask = asks[i / 2];
            QUrl url(base + QStringLiteral("search"));
            QUrlQuery q;
            if (i % 2 == 0)
            {
                q.addQueryItem(QStringLiteral("track_name"), ask.title);
                if (!ask.artist.isEmpty())
                    q.addQueryItem(QStringLiteral("artist_name"), ask.artist);
            }
            else if (!ask.artist.isEmpty())
                q.addQueryItem(QStringLiteral("q"), ask.artist + QLatin1Char(' ') + ask.title);
            else
                continue;
            url.setQuery(q);
            const QJsonArray found = QJsonDocument::fromJson(fetch(obj, url, &result.reached)).array();
            double best = 1e9, factor = 1;
            for (const QJsonValue &v : found)
            {
                const QJsonObject o = v.toObject();
                const QString text = lyricsOf(o);
                if (text.isEmpty())
                    continue;
                const double length = o.value(QStringLiteral("duration")).toDouble();
                const bool synced = !o.value(QStringLiteral("syncedLyrics")).toString().isEmpty();
                double score = song.seconds > 0 ? std::abs(length - song.seconds) : 0;
                double scale = 1;
                if (score > 15)
                {
                    /* another version: only for one sped up or slowed down,
                     * its times stretched to the file's length */
                    const double ratio = length > 0 ? song.seconds / length : 0;
                    if (!otherSpeed || ratio < 0.5 || ratio > 2)
                        continue;
                    scale = ratio;
                    score += 100;
                }
                if (!synced)
                    score += 1000;
                if (score < best)
                {
                    best = score;
                    result.lrc = text;
                    factor = scale;
                }
            }
            if (!result.lrc.isEmpty() && factor != 1)
                result.lrc = scaleLrc(result.lrc, factor);
        }

        if (!result.lrc.isEmpty() && !kept.isEmpty() && QDir().mkpath(QFileInfo(kept).path()))
        {
            QFile file(kept);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                file.write(result.lrc.toUtf8());
        }
        return result;
    }, [this, song](const Result &result) {
        if (result.lrc.isEmpty())
        {
            setState(result.reached ? NotFound : Failed);
            return;
        }
        apply(song.url, fromLrc(result.lrc));
        setState(Found);
    });
}

QByteArray LyricsFinder::fromLrc(const QString &lrc)
{
    static const QRegularExpression stamp(QStringLiteral("^\\[(\\d+):(\\d+(?:[.:]\\d+)?)\\]"));
    static const QRegularExpression offsetTag(QStringLiteral("^\\[offset:\\s*([+-]?\\d+)\\]"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tag(QStringLiteral("^\\[[a-zA-Z#]+:.*\\]$"));

    std::multimap<qint64, QString> timed;
    QStringList plain;
    qint64 offset = 0;
    const QStringList lines = lrc.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (QString line : lines)
    {
        line = line.trimmed();
        if (const auto m = offsetTag.match(line); m.hasMatch())
        {
            offset = m.captured(1).toLongLong();
            continue;
        }
        QList<qint64> times;
        for (auto m = stamp.match(line); m.hasMatch(); m = stamp.match(line))
        {
            const double seconds = m.captured(2).replace(QLatin1Char(':'), QLatin1Char('.')).toDouble();
            times << m.captured(1).toLongLong() * 60000 + qint64(std::lround(seconds * 1000));
            line = line.mid(m.capturedLength());
        }
        if (!times.isEmpty())
        {
            for (qint64 t : times)
                timed.emplace(t, line.trimmed());
        }
        else if (!tag.match(line).hasMatch())
            plain << line;
    }

    QByteArray raw;
    const auto add = [&raw](qint64 ms, const QString &text) {
        raw += QByteArray::number(ms) + '\x1F' + text.toUtf8() + '\x1E';
    };
    if (!timed.empty())
    {
        for (const auto &[ms, text] : timed)
            add(std::max<qint64>(0, ms - offset), text);
    }
    else
    {
        while (!plain.isEmpty() && plain.last().isEmpty())
            plain.removeLast();
        while (!plain.isEmpty() && plain.first().isEmpty())
            plain.removeFirst();
        /* no times: far after any song's end, none ever current */
        qint64 ms = qint64(1) << 40;
        for (const QString &text : plain)
            add(ms++, text);
    }
    return raw;
}

}
