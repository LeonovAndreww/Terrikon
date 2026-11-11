/*****************************************************************************
 * media_tools.cpp: clip export, waveforms and thumbnails through ffmpeg
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

#include "qt.hpp"
#include "media_tools.hpp"

#include <vlc_configuration.h>

#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QSaveFile>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTextStream>
#include <QUrl>

#include <algorithm>
#include <cmath>

#ifdef _WIN32
# include <windows.h>
#endif

namespace terrikon {

QString findTool(const QString &name)
{
#ifdef _WIN32
    const QString exe = name + QStringLiteral(".exe");
#else
    const QString exe = name;
#endif
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString &candidate : { appDir.filePath(exe),
                                      appDir.filePath(QStringLiteral("tools/") + exe) })
    {
        if (QFileInfo(candidate).isExecutable())
            return candidate;
    }
    return QStandardPaths::findExecutable(name);
}

/* The video encoders of the ffmpeg found, asked once: builds differ (the
 * "essentials" one has libaom-av1, not libsvtav1) */
static const QStringList &ffmpegEncoders()
{
    static QStringList encoders;
    static bool asked = false;
    if (asked)
        return encoders;
    asked = true;
    const QString ffmpeg = findTool(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return encoders;
    QProcess process;
    process.start(ffmpeg, { QStringLiteral("-hide_banner"), QStringLiteral("-encoders") });
    if (!process.waitForFinished(5000))
    {
        process.kill();
        process.waitForFinished(1000);
        return encoders;
    }
    static const QRegularExpression line(QStringLiteral("^\\s*V\\S*\\s+(\\S+)"));
    for (const QString &text : QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n')))
        if (const auto match = line.match(text); match.hasMatch())
            encoders << match.captured(1);
    return encoders;
}

/* A process that cannot start sends errorOccurred(FailedToStart) and never
 * finished(): the finished handlers, which all take a crash as a failure,
 * are run anyway, so nothing waits for it forever */
static QProcess *watched(QProcess *process)
{
    QObject::connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            emit process->finished(-1, QProcess::CrashExit);
    });
    return process;
}

/* Whether MP4 can take every stream a copy would keep, as ffprobe sees
 * them; true when it cannot tell (MP4 then, as before) */
static bool fitsMp4(const QString &input, bool audioCopied)
{
    const QString ffprobe = findTool(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty())
        return true;
    QProcess process;
    process.start(ffprobe, { QStringLiteral("-v"), QStringLiteral("error"),
                             QStringLiteral("-show_entries"), QStringLiteral("stream=codec_type,codec_name:stream_disposition=attached_pic"),
                             QStringLiteral("-of"), QStringLiteral("csv=p=0"), input });
    if (!process.waitForFinished(4000))
    {
        process.kill();
        process.waitForFinished(1000);
        return true;
    }
    static const QStringList videoOk = { QStringLiteral("h264"), QStringLiteral("hevc"), QStringLiteral("av1"),
                                         QStringLiteral("mpeg4"), QStringLiteral("vp9") };
    static const QStringList audioOk = { QStringLiteral("aac"), QStringLiteral("mp3"), QStringLiteral("opus"),
                                         QStringLiteral("flac"), QStringLiteral("ac3"), QStringLiteral("eac3"),
                                         QStringLiteral("alac") };
    bool videoSeen = false;
    for (const QString &line : QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n')))
    {
        const QStringList f = line.trimmed().split(QLatin1Char(','));
        if (f.size() < 2)
            continue;
        /* codec_name,codec_type[,attached_pic] */
        const QString &codec = f[0], &type = f[1];
        const bool cover = f.size() > 2 && f[2] == QLatin1String("1");
        if (type == QLatin1String("video") && !cover && !videoSeen)
        {
            videoSeen = true;
            if (!videoOk.contains(codec))
                return false;
        }
        else if (type == QLatin1String("audio") && audioCopied && !audioOk.contains(codec))
            return false;
    }
    return true;
}

/* The software encoder used for a codec, empty when the ffmpeg found has
 * none. Hardware ones are left out: whether they work depends on the
 * graphics card, not on the build. */
static QString videoEncoder(const QString &codec)
{
    const QStringList &encoders = ffmpegEncoders();
    const QStringList wanted = codec == QLatin1String("av1")
        ? QStringList{ QStringLiteral("libsvtav1"), QStringLiteral("libaom-av1") }
        : codec == QLatin1String("hevc") ? QStringList{ QStringLiteral("libx265") }
                                         : QStringList{ QStringLiteral("libx264") };
    for (const QString &name : wanted)
        if (encoders.contains(name))
            return name;
    /* Asking failed: try the usual one rather than nothing */
    return encoders.isEmpty() ? wanted.first() : QString();
}

QString cacheDirFor(const QString &mediaPath, const QString &kind)
{
    const QFileInfo info(mediaPath);
    const QByteArray key = info.absoluteFilePath().toUtf8() + '|'
                         + QByteArray::number(info.size()) + '|'
                         + QByteArray::number(info.lastModified().toMSecsSinceEpoch());
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16));

    char *base = config_GetUserDir(VLC_CACHE_DIR);
    const QString root = base ? QString::fromUtf8(base) : QDir::tempPath();
    free(base);

    const QString dir = QDir(root).filePath(QStringLiteral("cache/%1/%2").arg(kind, hash));
    QDir().mkpath(dir);
    return dir;
}

static QString seconds(double value)
{
    return QString::number(value, 'f', 3);
}

// ClipExporter

ClipExporter::ClipExporter(QObject *parent)
    : QObject(parent)
{
}

ClipExporter::~ClipExporter()
{
    stopMeasuring();
    if (m_process)
    {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(2000);
        if (!m_plan.part.isEmpty())
            QFile::remove(m_plan.part);
    }
}

ClipExporter::Plan ClipExporter::plan(const QVariantMap &params, const QString &passLogPrefix)
{
    Plan p;

    QString input = params.value(QStringLiteral("input")).toString();
    /* The player hands out URLs; a plain Windows path is not one (its drive
     * letter would parse as a scheme, but not as a local file URL) */
    if (const QUrl url(input); url.isLocalFile())
        input = url.toLocalFile();
    const double start = params.value(QStringLiteral("start")).toDouble();
    const double end = params.value(QStringLiteral("end")).toDouble();
    const double length = end - start;
    if (input.isEmpty() || !QFileInfo::exists(input))
    {
        p.error = QStringLiteral("The source file was not found.");
        return p;
    }
    if (length <= 0.)
    {
        p.error = QStringLiteral("The clip end must be after its start.");
        return p;
    }

    const QString preset = params.value(QStringLiteral("preset"), QStringLiteral("copy")).toString();
    const bool sizeLimited = preset == QLatin1String("size");
    const bool vertical = params.value(QStringLiteral("vertical")).toBool();
    const bool merge = params.value(QStringLiteral("merge")).toBool();
    const bool copyVideo = preset == QLatin1String("copy") && !vertical;
    /* "h264", "hevc" or "av1" when re-encoding */
    const QString codec = params.value(QStringLiteral("codec"), QStringLiteral("h264")).toString();
    const bool hevc = codec == QLatin1String("hevc");
    const bool av1 = codec == QLatin1String("av1");

    struct Track { int index; double volume; };
    QVector<Track> tracks;
    bool audioChanged = false;
    const QVariantList trackList = params.value(QStringLiteral("tracks")).toList();
    for (const QVariant &value : trackList)
    {
        const QVariantMap track = value.toMap();
        if (!track.value(QStringLiteral("enabled"), true).toBool())
        {
            audioChanged = true;
            continue;
        }
        const double volume = std::clamp(track.value(QStringLiteral("volume"), 1.).toDouble(), 0., 2.);
        if (std::abs(volume - 1.) > 0.001)
            audioChanged = true;
        tracks.push_back({ track.value(QStringLiteral("index")).toInt(), volume });
    }
    if (merge && tracks.size() > 1)
        audioChanged = true;

    /* Without a track list every audio track is kept as is; their count is
     * unknown here, so plan for two (the usual game + microphone) */
    const bool allAudio = trackList.isEmpty();
    const int outputTracks = allAudio ? 2 : (merge && tracks.size() > 1) ? 1 : tracks.size();

    /* Several audio tracks need Matroska; one fits MP4, which chat apps
     * play inline, unless copied streams are ones MP4 does not take (VP8,
     * Vorbis, PCM...) */
    const bool audioCopied = !audioChanged && preset != QLatin1String("size");
    const QString extension = outputTracks > 1 ? QStringLiteral("mkv")
        : (copyVideo && !fitsMp4(input, audioCopied)) ? QStringLiteral("mkv")
        : QStringLiteral("mp4");

    QString output = params.value(QStringLiteral("output")).toString();
    if (output.isEmpty())
    {
        const QFileInfo source(input);
        /* A clip made only to be pasted somewhere goes to a temporary
         * folder, not next to the recording */
        const QDir folder = params.value(QStringLiteral("temporary")).toBool()
            ? QDir(QDir::temp().filePath(QStringLiteral("terrikon-clips")))
            : source.dir();
        QDir().mkpath(folder.absolutePath());
        const int s = int(start);
        const QString stamp = QStringLiteral("%1-%2-%3")
            .arg(s / 3600, 2, 10, QLatin1Char('0'))
            .arg(s / 60 % 60, 2, 10, QLatin1Char('0'))
            .arg(s % 60, 2, 10, QLatin1Char('0'));
        const QString base = folder.filePath(source.completeBaseName()
                                             + QStringLiteral("_clip_") + stamp);
        output = base + QLatin1Char('.') + extension;
        for (int i = 2; QFileInfo::exists(output); ++i)
            output = base + QStringLiteral("_%1.").arg(i) + extension;
    }
    p.output = output;

    /* Making the clip over its own source would destroy it */
    {
        const QString a = QFileInfo(input).canonicalFilePath();
        const QString b = QFileInfo(output).canonicalFilePath();
#ifdef _WIN32
        const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
        if (!b.isEmpty() && a.compare(b, cs) == 0)
        {
            p.error = QStringLiteral("The clip cannot replace the video it is made from.");
            return p;
        }
    }
    /* ffmpeg writes a file of its own next to the output (same disk, so
     * the rename at the end is a rename); the output only appears, or is
     * replaced, once the clip is complete */
    {
        const QFileInfo out(output);
        const QString tag = QString::number(QDateTime::currentMSecsSinceEpoch(), 36)
                          + QString::number(QCoreApplication::applicationPid(), 36);
        p.part = out.dir().filePath(out.completeBaseName() + QStringLiteral(".part-") + tag
                                    + QLatin1Char('.') + out.suffix());
    }

    const QString encoder = copyVideo ? QString() : videoEncoder(codec);
    if (!copyVideo && encoder.isEmpty())
    {
        p.error = QStringLiteral("This ffmpeg has no encoder for this codec.");
        return p;
    }

    int audioKbps = 160;
    int videoKbps = 0;
    if (sizeLimited)
    {
        /* Aim a little below the limit: the container adds some. The rate
         * is held under videoKbps over any second, so a second's worth of
         * buffer is the most the whole clip can go over */
        const double target = params.value(QStringLiteral("targetBytes"),
                                           20. * 1024 * 1024).toDouble() * 0.95;
        audioKbps = 128;
        const double otherKbps = audioKbps * std::max(outputTracks, 0) + 30;
        videoKbps = int((target * 8. / 1000. - otherKbps * length) / (length + 1.));
        /* A second try after the first came out too big */
        videoKbps = int(videoKbps * std::clamp(params.value(QStringLiteral("rateScale"), 1.).toDouble(), .3, 1.));
        if (videoKbps < 150)
        {
            p.error = QStringLiteral("The clip is too long for this file size.");
            return p;
        }
    }

    QStringList videoFilters;
    if (vertical)
    {
        const double cropX = std::clamp(params.value(QStringLiteral("cropX"), 0.5).toDouble(), 0., 1.);
        const double cropY = std::clamp(params.value(QStringLiteral("cropY"), 0.5).toDouble(), 0., 1.);
        const double zoom = std::clamp(params.value(QStringLiteral("cropZoom"), 1.).toDouble(), 1., 4.);
        /* A 9:16 window, as large as fits at zoom 1 (full height, or full
         * width of a source narrower than 9:16), smaller when zoomed in,
         * placed by cropX/cropY within what is left around it. Anamorphic
         * sources are made square-pixel first, so 9:16 is what is seen. */
        const QString w = QStringLiteral("min(iw\\,ih*9/16)/%1").arg(zoom, 0, 'f', 3);
        const QString h = QStringLiteral("min(iw\\,ih*9/16)*16/9/%1").arg(zoom, 0, 'f', 3);
        videoFilters << QStringLiteral("scale=trunc(iw*sar/2)*2:ih,setsar=1")
                     << QStringLiteral("crop=%1:%2:(iw-%1)*%3:(ih-%2)*%4")
                            .arg(w, h).arg(cropX, 0, 'f', 3).arg(cropY, 0, 'f', 3)
                     << ((sizeLimited && videoKbps < 2500) ? QStringLiteral("scale=720:1280")
                                                          : QStringLiteral("scale=1080:1920"));
    }
    else if (sizeLimited)
    {
        /* Lower the resolution rather than starving the encoder */
        videoFilters << (videoKbps < 600  ? QStringLiteral("scale=-2:'min(ih,480)'")
                       : videoKbps < 1200 ? QStringLiteral("scale=-2:'min(ih,720)'")
                                          : QStringLiteral("scale=-2:'min(ih,1080)'"));
    }

    const QStringList head = {
        QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-n"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"), QStringLiteral("-nostats"),
        QStringLiteral("-ss"), seconds(start), QStringLiteral("-i"), input,
        QStringLiteral("-t"), seconds(length),
    };

    QStringList video = { QStringLiteral("-map"), QStringLiteral("0:V:0?") };
    if (!videoFilters.isEmpty())
        video << QStringLiteral("-vf") << videoFilters.join(QLatin1Char(','));
    if (copyVideo)
        video << QStringLiteral("-c:v") << QStringLiteral("copy")
              << QStringLiteral("-avoid_negative_ts") << QStringLiteral("make_zero");
    else
    {
        /* Similar visual quality at each codec's own scale */
        const QString crf = av1 ? QStringLiteral("32") : hevc ? QStringLiteral("24")
                                                              : QStringLiteral("20");
        const bool aom = encoder == QLatin1String("libaom-av1");
        video << QStringLiteral("-c:v") << encoder;
        if (encoder == QLatin1String("libsvtav1"))
            video << QStringLiteral("-preset") << QStringLiteral("8");
        else if (aom)
            video << QStringLiteral("-cpu-used") << QStringLiteral("6")
                  << QStringLiteral("-row-mt") << QStringLiteral("1");
        else
            video << QStringLiteral("-preset") << (hevc ? QStringLiteral("fast")
                                                        : QStringLiteral("veryfast"));
        /* players on Apple systems want this tag in MP4 */
        if (hevc)
            video << QStringLiteral("-tag:v") << QStringLiteral("hvc1");

        /* With a size limit: a clip that fits at full quality keeps it,
         * held under the rate the limit allows, and is not blown up to the
         * limit; one that does not fit (measured beforehand) gets the whole
         * rate the limit allows, which a quality ceiling would leave half
         * unused */
        const double bestBytes = params.value(QStringLiteral("bestBytes")).toDouble();
        const double targetBytes = params.value(QStringLiteral("targetBytes"), 20. * 1024 * 1024).toDouble();
        const bool squeeze = sizeLimited && bestBytes > targetBytes * 0.75;
        if (squeeze)
            video << QStringLiteral("-b:v") << QStringLiteral("%1k").arg(videoKbps)
                  << QStringLiteral("-maxrate") << QStringLiteral("%1k").arg(videoKbps * 4 / 3)
                  << QStringLiteral("-bufsize") << QStringLiteral("%1k").arg(videoKbps * 2);
        else
        {
            video << QStringLiteral("-crf") << crf;
            if (aom)
                /* libaom: the rate is a ceiling under -crf, 0 for none */
                video << QStringLiteral("-b:v") << (sizeLimited ? QStringLiteral("%1k").arg(videoKbps)
                                                                : QStringLiteral("0"));
            else if (sizeLimited)
                video << QStringLiteral("-maxrate") << QStringLiteral("%1k").arg(videoKbps)
                      << QStringLiteral("-bufsize") << QStringLiteral("%1k").arg(videoKbps);
        }
        video << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
    }

    QStringList audio;
    QStringList graph;
    if (audioChanged)
    {
        QString labels;
        for (int i = 0; i < tracks.size(); ++i)
        {
            graph << QStringLiteral("[0:a:%1]volume=%2[a%3]")
                         .arg(tracks[i].index).arg(tracks[i].volume, 0, 'f', 3).arg(i);
            labels += QStringLiteral("[a%1]").arg(i);
        }
        if (merge && tracks.size() > 1)
        {
            /* normalize=0 keeps each track at the volume asked for; the
             * limiter catches the peaks the sum can then reach */
            graph << labels + QStringLiteral("amix=inputs=%1:normalize=0:duration=longest,"
                                             "alimiter=limit=0.95[aout]").arg(tracks.size());
            audio << QStringLiteral("-map") << QStringLiteral("[aout]");
        }
        else
        {
            for (int i = 0; i < tracks.size(); ++i)
                audio << QStringLiteral("-map") << QStringLiteral("[a%1]").arg(i);
        }
    }
    else if (allAudio)
        audio << QStringLiteral("-map") << QStringLiteral("0:a?");
    else
        for (const Track &track : tracks)
            audio << QStringLiteral("-map") << QStringLiteral("0:a:%1").arg(track.index);

    if (!audioChanged && !sizeLimited)
        audio << QStringLiteral("-c:a") << QStringLiteral("copy");
    else
        audio << QStringLiteral("-c:a") << QStringLiteral("aac")
              << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(audioKbps);

    if (!graph.isEmpty())
        audio = QStringList{ QStringLiteral("-filter_complex"), graph.join(QLatin1Char(';')) } + audio;

    QStringList tail;
    if (extension == QLatin1String("mp4"))
        tail << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    (void) passLogPrefix;
    QStringList single = head;
    single << video << audio << tail << p.part;
    p.passes << single;
    return p;
}

bool ClipExporter::canEncode(const QString &codec) const
{
    return !videoEncoder(codec).isEmpty();
}

qint64 ClipExporter::sourceSize(const QString &path) const
{
    if (path.isEmpty())
        return 0;
    const auto it = m_sourceSizes.constFind(path);
    if (it != m_sourceSizes.constEnd())
        return *it;
    m_sourceSizes.insert(path, -1);
    auto self = const_cast<ClipExporter *>(this);
    offThread(self, [path] {
        const QFileInfo info(path);
        return info.exists() ? info.size() : qint64(0);
    }, [self, path](qint64 size) {
        self->m_sourceSizes.insert(path, size);
        ++self->m_sourceRevision;
        emit self->sourceChanged();
        if (self->m_measureWaiting)
            self->measure(self->m_measureParams);
    });
    return -1;
}

bool ClipExporter::start(const QVariantMap &params)
{
    if (m_process)
        return false;

    if (findTool(QStringLiteral("ffmpeg")).isEmpty())
    {
        emit finished(false, {}, QStringLiteral("ffmpeg was not found."));
        return false;
    }

    if (m_starting)
        return false;

    m_tmp = std::make_unique<QTemporaryDir>();
    QVariantMap planned = params;
    if (!planned.contains(QStringLiteral("bestBytes")) && m_bestBytes > 0.)
        planned.insert(QStringLiteral("bestBytes"), m_bestBytes);
    m_params = planned;
    m_retried = false;
    m_starting = true;
    emit runningChanged();
    /* making the plan looks at the files and may run ffprobe: not on the
     * interface thread */
    const quint64 start = ++m_starts;
    const QString prefix = m_tmp->filePath(QStringLiteral("x264"));
    offThread(this, [planned, prefix] { return plan(planned, prefix); },
              [this, start](const Plan &p) {
        if (start != m_starts)
            return;
        m_starting = false;
        started(p);
    });
    return true;
}

void ClipExporter::started(const Plan &plan)
{
    m_plan = plan;
    if (!m_plan.error.isEmpty())
    {
        m_tmp.reset();
        emit runningChanged();
        emit finished(false, {}, m_plan.error);
        return;
    }
    const QVariantMap &params = m_params;

    m_length = params.value(QStringLiteral("end")).toDouble()
             - params.value(QStringLiteral("start")).toDouble();
    m_targetBytes = params.value(QStringLiteral("preset")).toString() == QLatin1String("size")
                  ? params.value(QStringLiteral("targetBytes")).toDouble() : 0.;
    m_copyToClipboard = params.value(QStringLiteral("copyToClipboard")).toBool();
    m_pass = 0;
    m_errors.clear();
    setProgress(0.);
    runPass();
    emit runningChanged();
}

double ClipExporter::estimateBytes(const QVariantMap &params) const
{
    QString input = params.value(QStringLiteral("input")).toString();
    if (const QUrl url(input); url.isLocalFile())
        input = url.toLocalFile();
    const double length = params.value(QStringLiteral("end")).toDouble()
                        - params.value(QStringLiteral("start")).toDouble();
    const double mediaLength = params.value(QStringLiteral("mediaLength")).toDouble();
    const qint64 sourceBytes = sourceSize(input);
    if (length <= 0. || mediaLength <= 0. || sourceBytes <= 0)
        return 0.;

    const QString preset = params.value(QStringLiteral("preset"), QStringLiteral("copy")).toString();

    /* The source's average rate; copying keeps it */
    const double sourceRate = double(sourceBytes) / mediaLength;
    if (preset == QLatin1String("copy") && !params.value(QStringLiteral("vertical")).toBool())
        return sourceRate * length;

    /* A rough guess only, until a few pieces are encoded: re-encoding
     * at the quality setting lands near the rate of the source (a phone or
     * a game recorder already spends what the picture needs), H.265 and
     * AV1 needing about 60% and 50% of what H.264 does */
    const QString codec = params.value(QStringLiteral("codec"), QStringLiteral("h264")).toString();
    const double codecFactor = codec == QLatin1String("av1") ? 0.5
                             : codec == QLatin1String("hevc") ? 0.6 : 1.;
    const double guess = sourceRate * codecFactor * length;
    if (preset == QLatin1String("size"))
        return std::min(guess, params.value(QStringLiteral("targetBytes")).toDouble() * 0.95);
    return guess;
}

void ClipExporter::stopMeasuring()
{
    if (!m_meter)
        return;
    m_meter->disconnect(this);
    m_meter->kill();
    m_meter->waitForFinished(1000);
    m_meter->deleteLater();
    m_meter = nullptr;
}

static void lowPriority(QProcess *process)
{
#ifdef _WIN32
    /* Measuring should never slow playback down */
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= BELOW_NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW;
    });
#else
    (void) process;
#endif
}

void ClipExporter::measure(const QVariantMap &params)
{
    stopMeasuring();
    m_measuredBytes = 0.;
    m_bestBytes = 0.;
    m_measureParams = params;

    QString input = params.value(QStringLiteral("input")).toString();
    if (const QUrl url(input); url.isLocalFile())
        input = url.toLocalFile();
    const double start = params.value(QStringLiteral("start")).toDouble();
    const double end = params.value(QStringLiteral("end")).toDouble();
    const double length = end - start;
    const QString preset = params.value(QStringLiteral("preset"), QStringLiteral("copy")).toString();
    const qint64 sourceBytes = length > 0. ? sourceSize(input) : 0;
    m_measureWaiting = sourceBytes < 0; /* measured again once known */
    if (sourceBytes <= 0)
    {
        emit measuredChanged();
        return;
    }

    const bool vertical = params.value(QStringLiteral("vertical")).toBool();
    if (preset == QLatin1String("copy") && !vertical)
    {
        /* Which audio tracks go in untouched, which get encoded */
        m_copiedAudio.clear();
        m_copiedAudioTracks = -1;
        m_extraAudioBytes = 0.;
        const QVariantList trackList = params.value(QStringLiteral("tracks")).toList();
        if (!trackList.isEmpty())
        {
            bool changed = params.value(QStringLiteral("merge")).toBool();
            QList<int> kept;
            for (const QVariant &value : trackList)
            {
                const QVariantMap track = value.toMap();
                if (!track.value(QStringLiteral("enabled"), true).toBool())
                    continue;
                if (std::abs(track.value(QStringLiteral("volume"), 1.).toDouble() - 1.) > 0.001)
                    changed = true;
                kept << track.value(QStringLiteral("index")).toInt();
            }
            if (changed)
            {
                /* Encoded at 160 kbit/s by start(), one track if merged */
                const int outputs = params.value(QStringLiteral("merge")).toBool() && kept.size() > 1
                                  ? 1 : kept.size();
                m_extraAudioBytes = outputs * 160e3 / 8. * length;
                m_copiedAudioTracks = 0;
            }
            else
            {
                m_copiedAudio = kept;
                m_copiedAudioTracks = kept.size();
            }
        }

        /* Reading from the keyframe before the start, as the copy does */
        m_meter = watched(new QProcess(this));
        m_meter->setProgram(findTool(QStringLiteral("ffprobe")));
        m_meter->setArguments({
            QStringLiteral("-v"), QStringLiteral("error"),
            QStringLiteral("-read_intervals"),
            QStringLiteral("%1%%2").arg(seconds(start), seconds(end)),
            QStringLiteral("-show_entries"), QStringLiteral("packet=codec_type,stream_index,pts_time,size"),
            QStringLiteral("-of"), QStringLiteral("csv=p=0"),
            input,
        });
        lowPriority(m_meter);
        connect(m_meter, &QProcess::finished, this, &ClipExporter::onProbed);
        m_meter->start();
        emit measuredChanged();
        return;
    }

    /* Re-encoding: the whole clip when short, else three pieces spread
     * over it, so both calm and busy parts count */
    m_samples.clear();
    m_sampledSeconds = 0.;
    m_sampledBytes = 0.;
    if (length <= 8.)
        m_samples << qMakePair(start, length);
    else
        for (double at : { 1. / 6., 3. / 6., 5. / 6. })
            m_samples << qMakePair(start + length * at - 1., 2.);
    m_meterTmp = std::make_unique<QTemporaryDir>();
    measureNextSample();
}

void ClipExporter::measureNextSample()
{
    if (m_samples.isEmpty())
    {
        const double length = m_measureParams.value(QStringLiteral("end")).toDouble()
                            - m_measureParams.value(QStringLiteral("start")).toDouble();
        m_measuredBytes = m_sampledSeconds > 0. ? m_sampledBytes / m_sampledSeconds * length : 0.;
        /* The pieces are encoded at full quality: what the clip would weigh
         * without a limit */
        m_bestBytes = m_measuredBytes;
        /* The pieces are too short for the limit to bite: it caps the
         * size of the whole, and only lowers it */
        if (m_measureParams.value(QStringLiteral("preset")).toString() == QLatin1String("size"))
            m_measuredBytes = std::min(m_measuredBytes,
                                       m_measureParams.value(QStringLiteral("targetBytes")).toDouble() * 0.95);
        if (m_meter)
            m_meter->deleteLater();
        m_meter = nullptr;
        emit measuredChanged();
        return;
    }

    const auto [start, length] = m_samples.takeFirst();
    QVariantMap params = m_measureParams;
    params[QStringLiteral("start")] = start;
    params[QStringLiteral("end")] = start + length;
    params[QStringLiteral("output")] = QString();
    params[QStringLiteral("temporary")] = false;
    const Plan p = plan(params, QString());
    if (!p.error.isEmpty() || p.passes.size() != 1 || !m_meterTmp || !m_meterTmp->isValid())
    {
        m_samples.clear();
        m_sampledSeconds = 0.;
        measureNextSample();
        return;
    }

    /* Same arguments, written to a scratch file instead */
    QStringList args = p.passes.first();
    m_sampleOutput = m_meterTmp->filePath(QStringLiteral("sample.") + QFileInfo(p.output).suffix());
    args.last() = m_sampleOutput;

    if (m_meter)
        m_meter->deleteLater();
    m_meter = watched(new QProcess(this));
    m_meter->setProgram(findTool(QStringLiteral("ffmpeg")));
    m_meter->setArguments(args);
    lowPriority(m_meter);
    connect(m_meter, &QProcess::finished, this, [this, length](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::NormalExit && exitCode == 0)
        {
            m_sampledBytes += QFileInfo(m_sampleOutput).size();
            m_sampledSeconds += length;
        }
        QFile::remove(m_sampleOutput);
        measureNextSample();
    });
    m_meter->start();
    emit measuredChanged();
}

void ClipExporter::onProbed(int exitCode, QProcess::ExitStatus status)
{
    QProcess *probe = m_meter;
    m_meter = nullptr;
    probe->deleteLater();
    if (status != QProcess::NormalExit || exitCode != 0)
    {
        m_measuredBytes = estimateBytes(m_measureParams);
        emit measuredChanged();
        return;
    }

    const double end = m_measureParams.value(QStringLiteral("end")).toDouble();
    /* Audio streams by order of their index: track N is the Nth of them */
    QList<int> audioStreams;
    struct Packet { bool video; int stream; double pts; qint64 size; };
    QList<Packet> packets;
    const QList<QByteArray> lines = probe->readAllStandardOutput().split('\n');
    for (const QByteArray &line : lines)
    {
        const QList<QByteArray> f = line.trimmed().split(',');
        if (f.size() < 4)
            continue;
        const bool video = f[0] == "video";
        if (!video && f[0] != "audio")
            continue;
        bool ok = false;
        const double pts = f[2].toDouble(&ok);
        if (!ok)
            continue;
        const int stream = f[1].toInt();
        if (!video && !audioStreams.contains(stream))
            audioStreams << stream;
        packets.push_back({ video, stream, pts, f[3].toLongLong() });
    }
    std::sort(audioStreams.begin(), audioStreams.end());

    double bytes = m_extraAudioBytes;
    for (const Packet &packet : packets)
    {
        if (packet.pts > end)
            continue;
        if (!packet.video)
        {
            if (m_copiedAudioTracks == 0)
                continue;
            if (m_copiedAudioTracks > 0 && !m_copiedAudio.contains(audioStreams.indexOf(packet.stream)))
                continue;
        }
        bytes += packet.size;
    }
    /* The container adds a little on top of the packets */
    m_measuredBytes = bytes * 1.01;
    emit measuredChanged();
}

QString ClipExporter::defaultOutput(const QVariantMap &params) const
{
    return plan(params, QString()).output;
}

QString ClipExporter::askSavePath(const QString &suggested) const
{
    const QString extension = QFileInfo(suggested).suffix();
    const QString filter = extension.isEmpty()
        ? QString()
        : QStringLiteral("%1 (*.%2)").arg(extension.toUpper(), extension);
    return QFileDialog::getSaveFileName(nullptr, qtr("Save clip as"), suggested, filter);
}

void ClipExporter::reveal(const QString &path) const
{
    if (path.isEmpty())
        return;
#ifdef _WIN32
    /* Explorer wants the path quoted after the comma, not the whole
     * argument, so bypass QProcess quoting */
    QProcess explorer;
    explorer.setProgram(QStringLiteral("explorer.exe"));
    explorer.setNativeArguments(QStringLiteral("/select,\"%1\"").arg(QDir::toNativeSeparators(path)));
    explorer.startDetached();
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

void ClipExporter::cancel()
{
    if (m_starting)
    {
        ++m_starts; /* its plan will be dropped */
        m_starting = false;
        m_tmp.reset();
        emit runningChanged();
        emit finished(false, {}, QStringLiteral("Cancelled."));
        return;
    }
    if (!m_process)
        return;
    m_errors = "Cancelled.";
    m_process->kill();
}

void ClipExporter::runPass()
{
    m_process = new QProcess(this);
    m_process->setProgram(findTool(QStringLiteral("ffmpeg")));
    m_process->setArguments(m_plan.passes.at(m_pass));
    connect(m_process, &QProcess::readyReadStandardOutput, this, &ClipExporter::onOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        m_errors += m_process->readAllStandardError();
        if (m_errors.size() > 4096)
            m_errors = m_errors.right(4096);
    });
    connect(m_process, &QProcess::finished, this, &ClipExporter::onFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        /* Not started: no finished() will come */
        if (error != QProcess::FailedToStart || !m_process)
            return;
        m_process->deleteLater();
        m_process = nullptr;
        finish(false, QStringLiteral("ffmpeg could not be started."));
    });
    m_elapsed.start();
    m_process->start();
}

void ClipExporter::onOutput()
{
    /* -progress writes key=value lines; out_time_us is the position
     * reached. A read can end in the middle of a line: keep that part. */
    static const QRegularExpression outTime(QStringLiteral("out_time_(?:us|ms)=(\\d+)\\n"));
    m_progressLine += m_process->readAllStandardOutput();
    const int end = m_progressLine.lastIndexOf('\n');
    if (end < 0)
        return;
    const QString text = QString::fromUtf8(m_progressLine.left(end + 1));
    m_progressLine.remove(0, end + 1);
    double reached = -1.;
    for (auto it = outTime.globalMatch(text); it.hasNext(); )
        reached = it.next().captured(1).toDouble() / 1e6;
    if (reached < 0. || m_length <= 0.)
        return;

    const double passes = m_plan.passes.size();
    setProgress((m_pass + std::clamp(reached / m_length, 0., 1.)) / passes);

    /* Time left at the pace so far, once there is a pace to go by */
    const double elapsed = m_elapsed.elapsed() / 1000.;
    const double left = m_progress > 0.02 && elapsed > 1.
                      ? elapsed * (1. - m_progress) / m_progress : -1.;
    if (left != m_secondsLeft)
    {
        m_secondsLeft = left;
        emit secondsLeftChanged();
    }
}

void ClipExporter::onFinished(int exitCode, QProcess::ExitStatus status)
{
    m_process->deleteLater();
    m_process = nullptr;

    if (status != QProcess::NormalExit || exitCode != 0)
    {
        const QString error = QString::fromUtf8(m_errors).trimmed();
        finish(false, error.isEmpty() ? QStringLiteral("ffmpeg failed.") : error);
        return;
    }

    if (++m_pass < m_plan.passes.size())
    {
        runPass();
        return;
    }
    /* The limit is aimed under, not promised by every encoder (libaom
     * goes over by some 15%): once, the clip is made again at a rate lowered
     * by as much as it went over. The file is the one this export has just
     * made (-n refuses an existing one), nothing else is removed. */
    const qint64 size = QFileInfo(m_plan.part).size();
    if (size <= 0)
    {
        finish(false, QStringLiteral("ffmpeg made an empty file."));
        return;
    }
    if (m_targetBytes > 0. && size > m_targetBytes && !m_retried && QFile::remove(m_plan.part))
    {
        m_retried = true;
        QVariantMap again = m_params;
        again.insert(QStringLiteral("bestBytes"), 1e18);
        again.insert(QStringLiteral("rateScale"), m_targetBytes / double(size) * 0.93);
        const Plan retry = plan(again, m_tmp->filePath(QStringLiteral("x264")));
        if (retry.error.isEmpty())
        {
            m_plan = retry;
            m_pass = 0;
            setProgress(0.);
            runPass();
            return;
        }
    }
    if (m_targetBytes > 0. && size > m_targetBytes)
        m_overLimitBytes = size;
    else
        m_overLimitBytes = 0;
    const QString error = publish();
    finish(error.isEmpty(), error);
}

/* Moves the finished part to the output. A file already there (Save As,
 * replacing confirmed) is set aside first and put back if the move fails. */
QString ClipExporter::publish()
{
    const QString &part = m_plan.part, &output = m_plan.output;
    if (!QFileInfo::exists(output))
        return QFile::rename(part, output) ? QString()
             : QStringLiteral("The clip could not be saved as %1.").arg(QDir::toNativeSeparators(output));
    const QString old = part + QStringLiteral(".old");
    if (!QFile::rename(output, old))
        return QStringLiteral("%1 could not be replaced (is it open somewhere?).").arg(QDir::toNativeSeparators(output));
    if (!QFile::rename(part, output))
    {
        QFile::rename(old, output);
        return QStringLiteral("The clip could not be saved as %1.").arg(QDir::toNativeSeparators(output));
    }
    QFile::remove(old);
    return {};
}

void ClipExporter::finish(bool ok, const QString &error)
{
    /* whatever this export made and did not publish; nothing else */
    if (!m_plan.part.isEmpty() && QFileInfo::exists(m_plan.part))
        QFile::remove(m_plan.part);
    m_progressLine.clear();
    m_secondsLeft = -1.;
    emit secondsLeftChanged();
    if (ok)
    {
        setProgress(1.);
        if (m_copyToClipboard)
        {
            /* As a file, so it can be pasted into a chat or a folder */
            auto mime = new QMimeData;
            mime->setUrls({ QUrl::fromLocalFile(m_plan.output) });
            QGuiApplication::clipboard()->setMimeData(mime);
        }
    }
    m_tmp.reset();
    emit runningChanged();
    emit finished(ok, ok ? m_plan.output : QString(), error);
}

void ClipExporter::setProgress(double progress)
{
    if (qFuzzyCompare(m_progress, progress))
        return;
    m_progress = progress;
    emit progressChanged();
}

// WaveformLoader

static constexpr int waveformSampleRate = 4000;

WaveformLoader::WaveformLoader(QObject *parent)
    : QObject(parent)
{
}

WaveformLoader::~WaveformLoader()
{
    stop();
}

void WaveformLoader::stop()
{
    if (!m_process)
        return;
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(2000);
    m_process->deleteLater();
    m_process = nullptr;
    emit loadingChanged();
}

void WaveformLoader::clear()
{
    ++m_loads;
    stop();
    m_path.clear();
    m_tracks.clear();
    m_loaded.clear();
    m_current = -1;
    emit tracksChanged();
}

void WaveformLoader::load(const QString &path)
{
    clear();
    if (path.isEmpty() || findTool(QStringLiteral("ffprobe")).isEmpty())
        return;
    m_path = path;
    const quint64 load = ++m_loads;
    offThread(this, [path] { return cacheDirFor(path, QStringLiteral("waveforms")); },
              [this, load](const QString &dir) {
        if (load != m_loads)
            return;
        m_cacheDir = dir;
        probe();
    });
}

void WaveformLoader::probe()
{
    m_process = watched(new QProcess(this));
    m_process->setProgram(findTool(QStringLiteral("ffprobe")));
    m_process->setArguments({ QStringLiteral("-v"), QStringLiteral("error"),
                              QStringLiteral("-select_streams"), QStringLiteral("a"),
                              QStringLiteral("-show_entries"), QStringLiteral("stream=index"),
                              QStringLiteral("-of"), QStringLiteral("csv=p=0"), m_path });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const QByteArray out = m_process->readAllStandardOutput();
        m_process->deleteLater();
        m_process = nullptr;

        int count = 0;
        if (status == QProcess::NormalExit && exitCode == 0)
            for (const QByteArray &line : out.split('\n'))
                if (!line.trimmed().isEmpty())
                    ++count;

        m_tracks = QVector<QVector<float>>(count);
        m_loaded = QVector<bool>(count, false);
        for (int track = 0; track < count; ++track)
        {
            QFile cache(cacheFile(track));
            if (!cache.open(QIODevice::ReadOnly))
                continue;
            const QByteArray data = cache.readAll();
            QVector<float> &peaks = m_tracks[track];
            peaks.resize(data.size() / int(sizeof(float)));
            memcpy(peaks.data(), data.constData(), peaks.size() * sizeof(float));
            m_loaded[track] = true;
        }
        emit tracksChanged();
        for (int track = 0; track < count; ++track)
            if (m_loaded[track])
                emit trackLoaded(track);
        decodeNext();
    });
    m_process->start();
    emit loadingChanged();
}

QString WaveformLoader::cacheFile(int track) const
{
    return QDir(m_cacheDir).filePath(QStringLiteral("track-%1.f32").arg(track));
}

void WaveformLoader::decodeNext()
{
    m_current = -1;
    for (int track = 0; track < m_loaded.size(); ++track)
        if (!m_loaded[track])
        {
            m_current = track;
            break;
        }
    if (m_current < 0)
    {
        emit loadingChanged();
        return;
    }

    m_pending.clear();
    m_peak = 0.f;
    m_samplesInPeak = 0;
    m_tracks[m_current].clear();

    /* Mono, low rate 16-bit PCM is plenty for peaks and fast to decode */
    m_process = watched(new QProcess(this));
    m_process->setProgram(findTool(QStringLiteral("ffmpeg")));
    m_process->setArguments({ QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                              QStringLiteral("-loglevel"), QStringLiteral("error"),
                              QStringLiteral("-i"), m_path,
                              QStringLiteral("-map"), QStringLiteral("0:a:%1").arg(m_current),
                              QStringLiteral("-ac"), QStringLiteral("1"),
                              QStringLiteral("-ar"), QString::number(waveformSampleRate),
                              QStringLiteral("-f"), QStringLiteral("s16le"), QStringLiteral("pipe:1") });
    connect(m_process, &QProcess::readyReadStandardOutput, this, &WaveformLoader::onDecodeOutput);
    connect(m_process, &QProcess::finished, this, &WaveformLoader::onDecodeFinished);
    m_process->start();
}

void WaveformLoader::onDecodeOutput()
{
    static constexpr int samplesPerPeak = waveformSampleRate / peaksPerSecond;

    m_pending += m_process->readAllStandardOutput();
    const int samples = m_pending.size() / 2;
    const auto *data = reinterpret_cast<const unsigned char *>(m_pending.constData());
    QVector<float> &peaks = m_tracks[m_current];
    for (int i = 0; i < samples; ++i)
    {
        const int16_t sample = int16_t(data[2 * i] | (data[2 * i + 1] << 8));
        m_peak = std::max(m_peak, std::abs(sample) / 32768.f);
        if (++m_samplesInPeak == samplesPerPeak)
        {
            peaks.push_back(m_peak);
            m_peak = 0.f;
            m_samplesInPeak = 0;
        }
    }
    m_pending.remove(0, samples * 2);
}

void WaveformLoader::onDecodeFinished(int exitCode, QProcess::ExitStatus status)
{
    onDecodeOutput();
    m_process->deleteLater();
    m_process = nullptr;

    const int track = m_current;
    if (m_samplesInPeak > 0)
        m_tracks[track].push_back(m_peak);

    /* Even a failed track is marked done so the others still load */
    m_loaded[track] = true;
    if (status == QProcess::NormalExit && exitCode == 0)
    {
        QSaveFile cache(cacheFile(track));
        if (cache.open(QIODevice::WriteOnly))
        {
            cache.write(reinterpret_cast<const char *>(m_tracks[track].constData()),
                        m_tracks[track].size() * sizeof(float));
            cache.commit();
        }
    }
    emit trackLoaded(track);
    decodeNext();
}

bool WaveformLoader::isLoaded(int track) const
{
    return track >= 0 && track < m_loaded.size() && m_loaded[track];
}

QVariantList WaveformLoader::peaks(int track, int count) const
{
    QVariantList result;
    if (!isLoaded(track) || count <= 0)
        return result;

    const QVector<float> &source = m_tracks[track];
    if (source.isEmpty())
        return result;

    result.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        /* Each output bin keeps the loudest peak it covers */
        const qsizetype from = qsizetype(double(i) * source.size() / count);
        const qsizetype to = std::max(from + 1, qsizetype(double(i + 1) * source.size() / count));
        float peak = 0.f;
        for (qsizetype j = from; j < to && j < source.size(); ++j)
            peak = std::max(peak, source[j]);
        result.append(double(peak));
    }
    return result;
}

// ThumbnailStrip

ThumbnailStrip::ThumbnailStrip(QObject *parent)
    : QObject(parent)
{
}

ThumbnailStrip::~ThumbnailStrip()
{
    stop();
}

void ThumbnailStrip::stop()
{
    if (!m_process)
        return;
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(2000);
    m_process->deleteLater();
    m_process = nullptr;
}

void ThumbnailStrip::clear()
{
    ++m_loads;
    stop();
    m_times.clear();
    m_path.clear();
    m_cacheDir.clear();
    m_sheetDir.clear();
    if (m_ready)
    {
        m_ready = false;
        emit readyChanged();
    }
}

static QString denseDir(const QString &cacheDir)
{
    return QDir(cacheDir).filePath(QStringLiteral("dense"));
}

void ThumbnailStrip::load(const QString &path)
{
    clear();
    if (path.isEmpty())
        return;

    m_path = path;
    const quint64 load = ++m_loads;
    offThread(this, [path] { return cacheDirFor(path, QStringLiteral("thumbnails")); },
              [this, load](const QString &dir) {
        if (load == m_loads)
            loadFrom(dir);
    });
}

void ThumbnailStrip::loadFrom(const QString &dir)
{
    m_cacheDir = dir;
    if (readIndex(denseDir(m_cacheDir)))
    {
        m_ready = true;
        emit readyChanged();
        return;
    }
    if (readIndex(m_cacheDir))
    {
        m_ready = true;
        emit readyChanged();
        startPass(true);
        return;
    }
    startPass(false);
}

void ThumbnailStrip::startPass(bool dense)
{
    const QString ffmpeg = findTool(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return;

    const QString dir = dense ? denseDir(m_cacheDir) : m_cacheDir;
    QDir().mkpath(dir);

    /* At most one frame per spacing, letterboxed into equal cells; showinfo
     * logs each kept frame's time in the same order as the cells */
    const QString filters = QStringLiteral(
        "select='isnan(prev_selected_t)+gte(t-prev_selected_t,%1)',"
        "scale=%2:%3:force_original_aspect_ratio=decrease,"
        "pad=%2:%3:(ow-iw)/2:(oh-ih)/2,showinfo,tile=%4x%5")
        .arg(dense ? denseSpacing : keySpacing)
        .arg(cellWidth).arg(cellHeight).arg(columns).arg(rows);

    QStringList args = { QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"),
                         QStringLiteral("-y"), QStringLiteral("-loglevel"), QStringLiteral("info") };
    if (dense)
        /* Every frame is decoded: let the GPU do it where it can */
        args << QStringLiteral("-hwaccel") << QStringLiteral("auto");
    else
        /* Key frames only: no real decoding, ready in a moment */
        args << QStringLiteral("-skip_frame") << QStringLiteral("nokey");
    args << QStringLiteral("-i") << m_path << QStringLiteral("-an") << QStringLiteral("-sn")
         << QStringLiteral("-vf") << filters
         << QStringLiteral("-fps_mode") << QStringLiteral("vfr")
         << QStringLiteral("-q:v") << QStringLiteral("5")
         << QDir(dir).filePath(QStringLiteral("sheet-%03d.jpg"));

    m_densePass = dense;
    m_process = watched(new QProcess(this));
    m_process->setProgram(ffmpeg);
    m_process->setArguments(args);
#ifdef _WIN32
    /* The preview should never slow playback down */
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= BELOW_NORMAL_PRIORITY_CLASS;
    });
#endif
    connect(m_process, &QProcess::finished, this, &ThumbnailStrip::onFinished);
    m_process->start();
}

bool ThumbnailStrip::readIndex(const QString &dir)
{
    QFile index(QDir(dir).filePath(QStringLiteral("index.txt")));
    if (!index.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QVector<double> times;
    while (!index.atEnd())
    {
        bool ok = false;
        const double time = index.readLine().trimmed().toDouble(&ok);
        if (ok)
            times.push_back(time);
    }
    if (times.isEmpty() || !QFileInfo::exists(QDir(dir).filePath(QStringLiteral("sheet-001.jpg"))))
        return false;
    m_times = times;
    m_sheetDir = dir;
    return true;
}

void ThumbnailStrip::onFinished(int exitCode, QProcess::ExitStatus status)
{
    const QString log = QString::fromUtf8(m_process->readAllStandardError());
    m_process->deleteLater();
    m_process = nullptr;
    if (status != QProcess::NormalExit || exitCode != 0)
        return;

    const bool dense = m_densePass;
    const QString dir = dense ? denseDir(m_cacheDir) : m_cacheDir;
    static const QRegularExpression ptsTime(QStringLiteral("Parsed_showinfo.*pts_time:\\s*([-\\d.]+)"));
    QFile index(QDir(dir).filePath(QStringLiteral("index.txt")));
    if (!index.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream out(&index);
    for (auto it = ptsTime.globalMatch(log); it.hasNext(); )
        out << it.next().captured(1) << '\n';
    index.close();

    if (readIndex(dir))
    {
        /* Also sent when the dense set replaces the quick one, so frames
         * asked for again come from it */
        m_ready = true;
        emit readyChanged();
    }
    if (!dense)
        startPass(true);
}

QVariantMap ThumbnailStrip::frameAt(double seconds) const
{
    if (!m_ready || m_times.isEmpty())
        return {};

    /* The last thumbnail at or before the asked time */
    auto it = std::upper_bound(m_times.cbegin(), m_times.cend(), seconds);
    const qsizetype i = it == m_times.cbegin() ? 0 : (it - m_times.cbegin()) - 1;

    const int perSheet = columns * rows;
    const int cell = int(i % perSheet);
    const QString sheet = QDir(m_sheetDir).filePath(
        QStringLiteral("sheet-%1.jpg").arg(int(i / perSheet) + 1, 3, 10, QLatin1Char('0')));
    return {
        { QStringLiteral("source"), QUrl::fromLocalFile(sheet).toString() },
        { QStringLiteral("x"), (cell % columns) * cellWidth },
        { QStringLiteral("y"), (cell / columns) * cellHeight },
        { QStringLiteral("width"), cellWidth },
        { QStringLiteral("height"), cellHeight },
        { QStringLiteral("time"), m_times[i] },
    };
}

}
