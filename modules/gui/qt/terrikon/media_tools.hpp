/*****************************************************************************
 * media_tools.hpp: clip export, waveforms and thumbnails through ffmpeg
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
#ifndef TERRIKON_MEDIA_TOOLS_HPP
#define TERRIKON_MEDIA_TOOLS_HPP

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVector>

#include <memory>

class QTemporaryDir;

#include <QCoreApplication>
#include <QPointer>
#include <QHash>
#include <thread>

namespace terrikon {

/* Path of the ffmpeg tool "name" ("ffmpeg", "ffprobe"): next to the
 * executable, in its "tools" folder, or in PATH. Empty if not found. */
QString findTool(const QString &name);

/* Cache folder for "kind" data of a media file, keyed by its path, size and
 * modification time so an edited file gets fresh data. Created on demand. */
QString cacheDirFor(const QString &mediaPath, const QString &kind);

/* Runs work() on a thread of its own, then done(result) back on the
 * interface thread if context still exists. For anything that looks at a
 * media file: on a network drive that does not answer, that takes as long
 * as Windows waits for it, and the window would stop responding. */
template<class Work, class Done>
void offThread(QObject *context, Work work, Done done)
{
    QPointer<QObject> ctx(context);
    std::thread([ctx, work, done] {
        auto result = work();
        QMetaObject::invokeMethod(qApp, [ctx, done, result] {
            if (ctx)
                done(result);
        }, Qt::QueuedConnection);
    }).detach();
}

/* Cuts a clip out of a local file, optionally re-mixing its audio tracks,
 * fitting it to a file size or cropping it to 9:16. */
class ClipExporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged FINAL)
    /* Size of the clip measure() was last asked about, in bytes; 0 while
     * unknown */
    Q_PROPERTY(double measuredBytes READ measuredBytes NOTIFY measuredChanged FINAL)
    Q_PROPERTY(bool measuring READ measuring NOTIFY measuredChanged FINAL)
    /* Seconds the clip being made still needs at the pace so far; -1 while
     * unknown */
    Q_PROPERTY(double secondsLeft READ secondsLeft NOTIFY secondsLeftChanged FINAL)
    /* Size of the last clip made when it came out over its size limit, else 0 */
    Q_PROPERTY(double overLimitBytes READ overLimitBytes NOTIFY finished FINAL)
    /* Changes when the size of a source file becomes known: read it in
     * bindings that call estimateBytes() so they are worked out again */
    Q_PROPERTY(int sourceRevision READ sourceRevision NOTIFY sourceChanged FINAL)

public:
    explicit ClipExporter(QObject *parent = nullptr);
    ~ClipExporter() override;

    /* params:
     *   input (local path), start and end (seconds)
     *   tracks: [{ index (audio stream number), volume (0..2), enabled }],
     *           all audio tracks unchanged if empty
     *   merge: mix the enabled tracks into one
     *   preset: "copy" (no video re-encoding), "quality" or "size"
     *   targetBytes: file size limit for "size"
     *   vertical, cropX, cropY (0..1), cropZoom (1..4): 9:16 crop; the
     *           position of the crop window and how much it is zoomed in
     *   output: destination path, next to the input if empty
     *   temporary: write to a temporary folder (for copying only)
     *   copyToClipboard: put the resulting file on the clipboard */
    Q_INVOKABLE bool start(const QVariantMap &params);
    Q_INVOKABLE void cancel();
    /* Rough size of the clip "params" would give, in bytes; 0 if unknown.
     * mediaLength (seconds) is the length of the whole input. */
    Q_INVOKABLE double estimateBytes(const QVariantMap &params) const;
    /* Finds out the size of the clip "params" would give, in the
     * background, into measuredBytes. Copying: the sizes of the packets
     * that would be copied, read from the file. Re-encoding: a few short
     * pieces of the clip are encoded with the same settings and the
     * result is scaled to the whole length. */
    Q_INVOKABLE void measure(const QVariantMap &params);
    /* Where "params" would be saved by default */
    Q_INVOKABLE QString defaultOutput(const QVariantMap &params) const;
    /* Asks where to save, starting from "suggested"; empty if cancelled */
    Q_INVOKABLE QString askSavePath(const QString &suggested) const;
    /* Shows a file in the system file manager, selected where possible */
    Q_INVOKABLE void reveal(const QString &path) const;
    /* Whether the ffmpeg found can encode "h264", "hevc" or "av1" */
    Q_INVOKABLE bool canEncode(const QString &codec) const;

    bool running() const { return m_process != nullptr || m_starting; }
    int sourceRevision() const { return m_sourceRevision; }
    double progress() const { return m_progress; }
    double measuredBytes() const { return m_measuredBytes; }
    bool measuring() const { return m_meter != nullptr || m_measureWaiting; }
    double secondsLeft() const { return m_secondsLeft; }
    double overLimitBytes() const { return double(m_overLimitBytes); }

    struct Plan
    {
        QList<QStringList> passes; /* ffmpeg arguments of each pass */
        QString output;
        QString part;   /* what ffmpeg writes, renamed to output when done */
        QString error;
    };
    static Plan plan(const QVariantMap &params, const QString &passLogPrefix);
    QString publish();

signals:
    void runningChanged();
    void progressChanged();
    void measuredChanged();
    void secondsLeftChanged();
    void finished(bool ok, const QString &output, const QString &error);
    void sourceChanged();

private:
    /* Size of a source file, 0 if it does not exist, -1 while it is being
     * looked up away from the interface thread (sourceChanged follows) */
    qint64 sourceSize(const QString &path) const;
    void started(const Plan &plan);
    void runPass();
    void onOutput();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void finish(bool ok, const QString &error);
    void setProgress(double progress);
    void stopMeasuring();
    void measureNextSample();
    void onProbed(int exitCode, QProcess::ExitStatus status);

    QProcess *m_process = nullptr;
    bool m_starting = false;      /* the plan is being made */
    quint64 m_starts = 0;
    mutable QHash<QString, qint64> m_sourceSizes;
    int m_sourceRevision = 0;
    bool m_measureWaiting = false; /* measure() waits for the source size */
    Plan m_plan;
    int m_pass = 0;
    double m_length = 0.;
    double m_progress = 0.;
    bool m_copyToClipboard = false;
    QByteArray m_errors;
    QByteArray m_progressLine;
    QElapsedTimer m_elapsed;
    double m_secondsLeft = -1.;
    double m_targetBytes = 0.;
    qint64 m_overLimitBytes = 0;
    std::unique_ptr<QTemporaryDir> m_tmp;

    QProcess *m_meter = nullptr;
    double m_measuredBytes = 0.;
    double m_bestBytes = 0.; /* full quality, measured, for the size limit */
    QVariantMap m_params;     /* of the clip being made */
    bool m_retried = false;
    QVariantMap m_measureParams;
    QList<QPair<double, double>> m_samples; /* start, length still to encode */
    double m_sampledSeconds = 0.;
    double m_sampledBytes = 0.;
    QString m_sampleOutput;
    std::unique_ptr<QTemporaryDir> m_meterTmp;
    int m_copiedAudioTracks = -1; /* -1: all, else how many are copied */
    QList<int> m_copiedAudio;
    double m_extraAudioBytes = 0.;
};

/* Peak levels of every audio track of a file, for drawing one waveform
 * per track. Tracks are decoded one after the other in the background. */
class WaveformLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int trackCount READ trackCount NOTIFY tracksChanged FINAL)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged FINAL)

public:
    /* One peak per 50 ms */
    static constexpr int peaksPerSecond = 20;

    explicit WaveformLoader(QObject *parent = nullptr);
    ~WaveformLoader() override;

    Q_INVOKABLE void load(const QString &path);
    /* Same for a media URL; anything but a local file clears */
    Q_INVOKABLE void loadUrl(const QUrl &url) { load(url.isLocalFile() ? url.toLocalFile() : QString()); }
    Q_INVOKABLE void clear();
    /* "count" peaks (0..1) covering the whole track, empty if not loaded */
    Q_INVOKABLE QVariantList peaks(int track, int count) const;
    Q_INVOKABLE bool isLoaded(int track) const;

    int trackCount() const { return m_tracks.size(); }
    bool loading() const { return m_process != nullptr; }

signals:
    void tracksChanged();
    void loadingChanged();
    void trackLoaded(int track);

private:
    void probe();
    void decodeNext();
    void onDecodeOutput();
    void onDecodeFinished(int exitCode, QProcess::ExitStatus status);
    void stop();
    QString cacheFile(int track) const;

    QString m_path;
    QString m_cacheDir;
    quint64 m_loads = 0;
    QVector<QVector<float>> m_tracks;
    QVector<bool> m_loaded;
    int m_current = -1;
    QProcess *m_process = nullptr;
    QByteArray m_pending;
    float m_peak = 0.f;
    int m_samplesInPeak = 0;
};

/* Thumbnails of a file, tiled into JPEG sheets, so the seek bar preview
 * can show a frame instantly while hovering. A quick pass over key frames
 * comes first; a second one, decoding everything, then gives a frame
 * every second and replaces it. Both are cached. */
class ThumbnailStrip : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged FINAL)
    Q_PROPERTY(int count READ count NOTIFY readyChanged FINAL)

public:
    static constexpr int cellWidth = 160;
    static constexpr int cellHeight = 90;
    static constexpr int columns = 8;
    static constexpr int rows = 8;
    /* Spacing of the quick key frame pass and of the dense pass */
    static constexpr double keySpacing = 2.;
    static constexpr double denseSpacing = 1.;

    explicit ThumbnailStrip(QObject *parent = nullptr);
    ~ThumbnailStrip() override;

    Q_INVOKABLE void load(const QString &path);
    /* Same for a media URL; anything but a local file clears */
    Q_INVOKABLE void loadUrl(const QUrl &url) { load(url.isLocalFile() ? url.toLocalFile() : QString()); }
    Q_INVOKABLE void clear();
    /* { source (file URL of the sheet), x, y, width, height, time } of the
     * thumbnail closest before "seconds", empty map if none */
    Q_INVOKABLE QVariantMap frameAt(double seconds) const;

    bool ready() const { return m_ready; }
    int count() const { return m_times.size(); }

signals:
    void readyChanged();

private:
    void loadFrom(const QString &dir);
    bool readIndex(const QString &dir);
    void startPass(bool dense);
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void stop();

    QString m_path;
    QString m_cacheDir;
    quint64 m_loads = 0;
    QString m_sheetDir; /* where the sheets in use are */
    bool m_densePass = false;
    QVector<double> m_times;
    bool m_ready = false;
    QProcess *m_process = nullptr;
};

}

#endif
