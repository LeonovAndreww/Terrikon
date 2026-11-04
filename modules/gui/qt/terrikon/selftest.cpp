/*****************************************************************************
 * selftest.cpp: developer self-test of the Terrikon media tools
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

#include "selftest.hpp"
#include "media_tools.hpp"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <memory>

namespace terrikon {

namespace {

/* Runs the steps one after the other and owns everything until the report
 * is written */
class SelfTest : public QObject
{
public:
    SelfTest(const QString &configPath, QObject *parent)
        : QObject(parent), m_configPath(configPath)
    {
        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly))
            m_config = QJsonDocument::fromJson(file.readAll()).object();
        m_input = m_config.value(QStringLiteral("input")).toString();
        m_report.insert(QStringLiteral("ffmpeg"), findTool(QStringLiteral("ffmpeg")));
        m_report.insert(QStringLiteral("ffprobe"), findTool(QStringLiteral("ffprobe")));
    }

    void start() { testWaveforms(); }

private:
    void testWaveforms()
    {
        m_timer.start();
        connect(&m_waveforms, &WaveformLoader::loadingChanged, this, [this] {
            if (m_waveforms.loading() || m_waveformsDone)
                return;
            m_waveformsDone = true;
            QJsonArray tracks;
            for (int track = 0; track < m_waveforms.trackCount(); ++track)
                tracks.append(QJsonArray::fromVariantList(m_waveforms.peaks(track, 30)));
            m_report.insert(QStringLiteral("waveforms"), QJsonObject{
                { QStringLiteral("tracks"), m_waveforms.trackCount() },
                { QStringLiteral("peaks"), tracks },
                { QStringLiteral("ms"), qint64(m_timer.elapsed()) },
            });
            QTimer::singleShot(0, this, [this] { testThumbnails(); });
        });
        m_waveforms.load(m_input);
    }

    void testThumbnails()
    {
        m_timer.start();
        const auto done = [this] {
            if (m_thumbnailsDone)
                return;
            m_thumbnailsDone = true;
            m_report.insert(QStringLiteral("thumbnails"), QJsonObject{
                { QStringLiteral("ready"), m_thumbnails.ready() },
                { QStringLiteral("count"), m_thumbnails.count() },
                { QStringLiteral("frameAt10"), QJsonObject::fromVariantMap(m_thumbnails.frameAt(10.)) },
                { QStringLiteral("ms"), qint64(m_timer.elapsed()) },
            });
            QTimer::singleShot(0, this, [this] { testClip(0); });
        };
        connect(&m_thumbnails, &ThumbnailStrip::readyChanged, this, done);
        QTimer::singleShot(120000, this, done);
        m_thumbnails.load(m_input);
        if (m_thumbnails.ready())
            done();
    }

    void testClip(int i)
    {
        const QJsonArray clips = m_config.value(QStringLiteral("clips")).toArray();
        if (i >= clips.size())
        {
            writeReport();
            return;
        }

        QVariantMap params = clips.at(i).toObject().toVariantMap();
        params.insert(QStringLiteral("input"), m_input);
        m_timer.start();

        auto exporter = std::make_shared<ClipExporter>();
        connect(exporter.get(), &ClipExporter::finished, this,
                [this, i, exporter](bool ok, const QString &output, const QString &error) {
            m_clips.append(QJsonObject{
                { QStringLiteral("ok"), ok },
                { QStringLiteral("output"), output },
                { QStringLiteral("error"), error },
                { QStringLiteral("bytes"), ok ? QFileInfo(output).size() : 0 },
                { QStringLiteral("ms"), qint64(m_timer.elapsed()) },
            });
            QTimer::singleShot(0, this, [this, i, exporter] { testClip(i + 1); });
        });
        exporter->start(params);
    }

    void writeReport()
    {
        m_report.insert(QStringLiteral("clips"), m_clips);
        QFile out(m_configPath + QStringLiteral(".result.json"));
        if (out.open(QIODevice::WriteOnly))
            out.write(QJsonDocument(m_report).toJson());
        deleteLater();
    }

    QString m_configPath;
    QJsonObject m_config;
    QString m_input;
    QJsonObject m_report;
    QJsonArray m_clips;
    QElapsedTimer m_timer;
    WaveformLoader m_waveforms;
    ThumbnailStrip m_thumbnails;
    bool m_waveformsDone = false;
    bool m_thumbnailsDone = false;
};

}

void runSelfTestIfRequested(QObject *parent)
{
    const QString config = qEnvironmentVariable("TERRIKON_SELFTEST");
    if (config.isEmpty())
        return;
    auto test = new SelfTest(config, parent);
    QTimer::singleShot(0, test, [test] { test->start(); });
}

}
