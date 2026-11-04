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

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <memory>

class QTemporaryDir;

namespace terrikon {

/* Path of the ffmpeg tool "name" ("ffmpeg", "ffprobe"): next to the
 * executable, in its "tools" folder, or in PATH. Empty if not found. */
QString findTool(const QString &name);

/* Cache folder for "kind" data of a media file, keyed by its path, size and
 * modification time so an edited file gets fresh data. Created on demand. */
QString cacheDirFor(const QString &mediaPath, const QString &kind);

/* Cuts a clip out of a local file, optionally re-mixing its audio tracks,
 * fitting it to a file size or cropping it to 9:16. */
class ClipExporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged FINAL)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged FINAL)

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
     *   vertical, cropX (0..1): 9:16 crop, cropX is the horizontal position
     *   output: destination path, next to the input if empty
     *   copyToClipboard: put the resulting file on the clipboard */
    Q_INVOKABLE bool start(const QVariantMap &params);
    Q_INVOKABLE void cancel();

    bool running() const { return m_process != nullptr; }
    double progress() const { return m_progress; }

    struct Plan
    {
        QList<QStringList> passes; /* ffmpeg arguments of each pass */
        QString output;
        QString error;
    };
    static Plan plan(const QVariantMap &params, const QString &passLogPrefix);

signals:
    void runningChanged();
    void progressChanged();
    void finished(bool ok, const QString &output, const QString &error);

private:
    void runPass();
    void onOutput();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void finish(bool ok, const QString &error);
    void setProgress(double progress);

    QProcess *m_process = nullptr;
    Plan m_plan;
    int m_pass = 0;
    double m_length = 0.;
    double m_progress = 0.;
    bool m_copyToClipboard = false;
    QByteArray m_errors;
    std::unique_ptr<QTemporaryDir> m_tmp;
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
    QVector<QVector<float>> m_tracks;
    QVector<bool> m_loaded;
    int m_current = -1;
    QProcess *m_process = nullptr;
    QByteArray m_pending;
    float m_peak = 0.f;
    int m_samplesInPeak = 0;
};

/* Key frame thumbnails of a file, tiled into a few JPEG sheets, so the
 * seek bar preview can show a frame instantly while hovering. */
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
    /* At most one thumbnail every 2 seconds */
    static constexpr double minSpacing = 2.;

    explicit ThumbnailStrip(QObject *parent = nullptr);
    ~ThumbnailStrip() override;

    Q_INVOKABLE void load(const QString &path);
    Q_INVOKABLE void clear();
    /* { source (file URL of the sheet), x, y, width, height, time } of the
     * thumbnail closest before "seconds", empty map if none */
    Q_INVOKABLE QVariantMap frameAt(double seconds) const;

    bool ready() const { return m_ready; }
    int count() const { return m_times.size(); }

signals:
    void readyChanged();

private:
    bool readIndex();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void stop();

    QString m_cacheDir;
    QVector<double> m_times;
    bool m_ready = false;
    QProcess *m_process = nullptr;
};

}

#endif
