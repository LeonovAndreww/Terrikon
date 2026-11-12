/*****************************************************************************
 * components.cpp: the outside tools Terrikon uses (ffmpeg, yt-dlp)
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

#include "components.hpp"
#include "media_tools.hpp"

#include <QDateTime>
#include <QVBoxLayout>
#include <QUrl>
#include <QRegularExpression>
#include <QProgressBar>
#include <QHBoxLayout>
#include <QFrame>
#include <QDesktopServices>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace terrikon {

Components *Components::s_instance = nullptr;

namespace {
struct Spec
{
    const char *id;
    const char *name;
    const char *what;
    const char *url;     /* the file to download */
    bool zip;            /* ffmpeg comes in a zip, its programs in bin/ */
    QStringList exes;
};

const QList<Spec> &specs()
{
    static const QList<Spec> list = {
        { "ffmpeg", "FFmpeg", N_("Clips, waveforms and the frames of the seek bar"),
          /* the "essentials" release build, LGPL/GPL, about 90 MB */
          "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip", true,
          { QStringLiteral("ffmpeg.exe"), QStringLiteral("ffprobe.exe") } },
        { "yt-dlp", "yt-dlp", N_("Links to YouTube, TikTok, Bilibili and other sites"),
          "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe", false,
          { QStringLiteral("yt-dlp.exe") } },
    };
    return list;
}

const Spec *specOf(const QString &id)
{
    for (const Spec &s : specs())
        if (id == QLatin1String(s.id))
            return &s;
    return nullptr;
}

QString systemTool(const char *name)
{
    /* curl and tar come with Windows 10 and later */
    return QDir(qEnvironmentVariable("SystemRoot", QStringLiteral("C:\\Windows")))
        .filePath(QStringLiteral("System32/%1.exe").arg(QLatin1String(name)));
}
}

Components::Components(qt_intf_t *intf, QObject *parent)
    : QObject(parent), m_intf(intf)
{
    s_instance = this;
    /* yt-dlp has to follow the sites: once a day, quietly */
    QTimer::singleShot(15000, this, &Components::updateYtdlpInBackground);
    QTimer::singleShot(3000, this, &Components::installAskedByInstaller);
}

/* Asked for by the installer (its "Components" box): the tools are
 * downloaded at the first start, with the window showing how far */
void Components::installAskedByInstaller()
{
    QSettings installer(QStringLiteral("HKEY_CURRENT_USER\\Software\\Terrikon"), QSettings::NativeFormat);
    if (installer.value(QStringLiteral("InstallComponents")).toString() != QLatin1String("1"))
        return;
    installer.setValue(QStringLiteral("InstallComponents"), QStringLiteral("0"));
    bool any = false;
    for (const char *id : { "ffmpeg", "yt-dlp" })
    {
        const QString tool = QString::fromLatin1(id);
        if (!installed(tool))
        {
            install(tool);
            any = true;
        }
    }
    if (any)
        showDialog();
}

Components *Components::instance()
{
    return s_instance;
}

QString Components::toolsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
         + QStringLiteral("/terrikon/tools");
}

bool Components::installed(const QString &id) const
{
    const Spec *spec = specOf(id);
    if (!spec)
        return false;
    for (const QString &exe : spec->exes)
        if (findTool(QFileInfo(exe).completeBaseName()).isEmpty())
            return false;
    return true;
}

QString Components::version(const QString &id) const
{
    if (m_versions.contains(id))
        return m_versions.value(id);
    const QString tool = findTool(id);
    if (tool.isEmpty())
        return {};
    QProcess process;
    process.start(tool, { id == QLatin1String("yt-dlp") ? QStringLiteral("--version")
                                                          : QStringLiteral("-version") });
    QString text;
    if (process.waitForFinished(4000))
        text = QString::fromUtf8(process.readAllStandardOutput()).section(QLatin1Char('\n'), 0, 0).trimmed();
    else
        process.kill();
    /* "ffmpeg version 8.0-essentials_build-www.gyan.dev Copyright..." */
    if (text.startsWith(QLatin1String("ffmpeg version ")))
        text = text.mid(15).section(QLatin1Char(' '), 0, 0).section(QLatin1Char('-'), 0, 0);
    m_versions.insert(id, text);
    return text;
}

void Components::install(const QString &id)
{
    const Spec *spec = specOf(id);
    if (!spec || m_busy.contains(id))
        return;
    m_busy.insert(id, true);
    emit changed(id);

    QDir().mkpath(toolsDir());
    const QString download = QDir(toolsDir()).filePath(
        QStringLiteral("%1.download%2").arg(id, spec->zip ? QStringLiteral(".zip") : QStringLiteral(".exe")));
    QFile::remove(download);

    auto curl = new QProcess(this);
    curl->setProgram(systemTool("curl"));
    curl->setArguments({ QStringLiteral("-L"), QStringLiteral("-f"),
                         QStringLiteral("--retry"), QStringLiteral("2"),
                         QStringLiteral("-o"), download, QString::fromLatin1(spec->url) });
    m_progress.insert(id, -1.);
    m_progressText.remove(id);
    /* curl's meter: "% Total % Received ...", rows rewritten after \r */
    connect(curl, &QProcess::readyReadStandardError, this, [this, id, curl] {
        const QString text = QString::fromUtf8(curl->readAllStandardError());
        curl->setProperty("log", QVariant(QString(curl->property("log").toString() + text)));
        const QStringList rows = text.split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts);
        for (auto it = rows.crbegin(); it != rows.crend(); ++it)
        {
            const QStringList f = it->simplified().split(QLatin1Char(' '));
            bool ok = false;
            const double percent = f.value(0).toDouble(&ok);
            if (!ok || f.size() < 4)
                continue;
            m_progress.insert(id, percent / 100.);
            /* "56.0M" of "98.1M" */
            m_progressText.insert(id, qtr("%1 of %2").arg(f.value(3).replace(QLatin1Char('M'), qtr(" MB")),
                                                          f.value(1).replace(QLatin1Char('M'), qtr(" MB"))));
            emit progressChanged(id);
            break;
        }
    });
    connect(curl, &QProcess::errorOccurred, this, [this, id, curl](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        curl->deleteLater();
        finish(id, qtr("curl, which comes with Windows, could not be started."));
    });
    connect(curl, &QProcess::finished, this, [this, id, spec, curl, download](int code, QProcess::ExitStatus status) {
        /* the message, not the meter */
        QString error;
        for (const QString &line : QString(curl->property("log").toString() + QString::fromUtf8(curl->readAllStandardError()))
                                       .split(QRegularExpression(QStringLiteral("[\\r\\n]")), Qt::SkipEmptyParts))
            if (line.startsWith(QLatin1String("curl:")))
                error = line.mid(5).trimmed();
        curl->deleteLater();
        if (status != QProcess::NormalExit || code != 0 || QFileInfo(download).size() <= 0)
        {
            QFile::remove(download);
            finish(id, error.isEmpty() ? qtr("The download failed.") : error);
            return;
        }
        if (!spec->zip)
        {
            /* replaced only once complete: a running copy keeps working */
            const QString target = QDir(toolsDir()).filePath(spec->exes.first());
            QFile::remove(target);
            finish(id, QFile::rename(download, target) ? QString() : qtr("The file could not be put in place."));
            return;
        }
        /* a zip: unpacked aside, its programs taken from bin/ */
        const QString unpacked = QDir(toolsDir()).filePath(id + QStringLiteral(".unpacked"));
        QDir(unpacked).removeRecursively();
        QDir().mkpath(unpacked);
        auto tar = new QProcess(this);
        tar->setProgram(systemTool("tar"));
        tar->setArguments({ QStringLiteral("-xf"), download, QStringLiteral("-C"), unpacked });
        connect(tar, &QProcess::finished, this, [this, id, spec, tar, download, unpacked](int code, QProcess::ExitStatus) {
            tar->deleteLater();
            QFile::remove(download);
            QString error;
            if (code != 0)
                error = qtr("The archive could not be unpacked.");
            for (const QString &exe : spec->exes)
            {
                if (!error.isEmpty())
                    break;
                QDirIterator it(unpacked, { exe }, QDir::Files, QDirIterator::Subdirectories);
                if (!it.hasNext())
                {
                    error = qtr("%1 is not in the archive.").arg(exe);
                    break;
                }
                const QString target = QDir(toolsDir()).filePath(exe);
                QFile::remove(target);
                if (!QFile::rename(it.next(), target))
                    error = qtr("The file could not be put in place.");
            }
            QDir(unpacked).removeRecursively();
            finish(id, error);
        });
        tar->start();
    });
    curl->start();
}

void Components::finish(const QString &id, const QString &error)
{
    m_busy.remove(id);
    m_progress.remove(id);
    m_progressText.remove(id);
    m_versions.remove(id);
    if (!error.isEmpty())
    {
        msg_Warn(m_intf, "%s could not be installed: %s", qtu(id), qtu(error));
        emit failed(id, error);
    }
    emit changed(id);
}

void Components::offer(const QString &id, const QString &why)
{
    const Spec *spec = specOf(id);
    if (!spec || installed(id) || m_busy.contains(id) || m_offered.value(id))
        return;
    m_offered.insert(id, true);
    QMessageBox box;
    box.setWindowTitle(QString::fromLatin1(spec->name));
    box.setIcon(QMessageBox::Question);
    box.setText(qtr("%1 needs %2, which is not installed.").arg(why, QString::fromLatin1(spec->name)));
    box.setInformativeText(qtr("Download it now? It is free software; it goes in the Terrikon folder of your user, and can be updated or removed from Tools > Components."));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::Yes);
    if (box.exec() == QMessageBox::Yes)
        install(id);
}

bool Components::ownCopy(const QString &id) const
{
    const QString tool = findTool(id);
    return !tool.isEmpty() && QFileInfo(tool).absolutePath() == QFileInfo(toolsDir()).absoluteFilePath();
}

void Components::remove(const QString &id)
{
    const Spec *spec = specOf(id);
    if (!spec || m_busy.contains(id))
        return;
    for (const QString &exe : spec->exes)
        QFile::remove(QDir(toolsDir()).filePath(exe));
    m_versions.remove(id);
    emit changed(id);
}

static QSettings componentSettings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("terrikon"), QStringLiteral("library"));
}

bool Components::autoUpdate()
{
    return componentSettings().value(QStringLiteral("Components/yt-dlp-auto-update"), true).toBool();
}

void Components::setAutoUpdate(bool on)
{
    componentSettings().setValue(QStringLiteral("Components/yt-dlp-auto-update"), on);
}

void Components::updateYtdlpInBackground()
{
    if (!autoUpdate())
        return;
    const QString ytdlp = QDir(toolsDir()).filePath(QStringLiteral("yt-dlp.exe"));
    if (!QFileInfo::exists(ytdlp) || m_busy.contains(QStringLiteral("yt-dlp")))
        return;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("terrikon"), QStringLiteral("library"));
    const QDateTime last = settings.value(QStringLiteral("Components/yt-dlp-checked")).toDateTime();
    if (last.isValid() && last.secsTo(QDateTime::currentDateTime()) < 24 * 3600)
        return;
    settings.setValue(QStringLiteral("Components/yt-dlp-checked"), QDateTime::currentDateTime());
    /* yt-dlp replaces its own file with the latest release */
    auto process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        msg_Dbg(m_intf, "yt-dlp -U: %s", qtu(QString::fromUtf8(process->readAllStandardOutput()).trimmed()));
        process->deleteLater();
        m_versions.remove(QStringLiteral("yt-dlp"));
        emit changed(QStringLiteral("yt-dlp"));
    });
    process->start(ytdlp, { QStringLiteral("-U") });
}

void Components::showDialog()
{
    if (m_dialog)
    {
        m_dialog->raise();
        m_dialog->activateWindow();
        return;
    }
    auto dialog = new QDialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(qtr("Components"));
    dialog->setMinimumWidth(620);
    dialog->setStyleSheet(QStringLiteral(
        "QFrame#card { border: 1px solid palette(mid); border-radius: 8px; }"
        "QLabel#name { font-size: 15px; font-weight: 600; }"
        "QLabel#muted { color: palette(placeholder-text); }"
        "QProgressBar { border: none; border-radius: 3px; background: palette(mid); max-height: 6px; }"
        "QProgressBar::chunk { border-radius: 3px; background: palette(highlight); }"));

    auto column = new QVBoxLayout(dialog);
    column->setContentsMargins(18, 16, 18, 16);
    column->setSpacing(12);
    auto intro = new QLabel(qtr("Programs Terrikon uses for some of its features. They are free software, downloaded from their authors."));
    intro->setWordWrap(true);
    column->addWidget(intro);

    for (const Spec &spec : specs())
    {
        const QString id = QString::fromLatin1(spec.id);
        auto card = new QFrame;
        card->setObjectName(QStringLiteral("card"));
        auto grid = new QGridLayout(card);
        grid->setContentsMargins(14, 12, 14, 12);
        grid->setHorizontalSpacing(12);
        grid->setVerticalSpacing(4);

        auto name = new QLabel(QString::fromLatin1(spec.name));
        name->setObjectName(QStringLiteral("name"));
        auto what = new QLabel(qtr(spec.what));
        what->setWordWrap(true);
        auto state = new QLabel;
        state->setObjectName(QStringLiteral("muted"));
        state->setWordWrap(true);
        state->setTextInteractionFlags(Qt::TextSelectableByMouse);
        auto bar = new QProgressBar;
        bar->setTextVisible(false);
        auto action = new QPushButton;
        auto removeButton = new QPushButton(qtr("Remove"));
        action->setMinimumWidth(110);

        grid->addWidget(name, 0, 0);
        grid->addWidget(what, 1, 0);
        grid->addWidget(state, 2, 0);
        grid->addWidget(bar, 3, 0, 1, 3);
        grid->addWidget(action, 0, 1, 2, 1, Qt::AlignVCenter);
        grid->addWidget(removeButton, 0, 2, 2, 1, Qt::AlignVCenter);
        grid->setColumnStretch(0, 1);

        if (id == QLatin1String("yt-dlp"))
        {
            auto autoBox = new QCheckBox(qtr("Keep it up to date by itself (sites change, old versions stop working)"));
            autoBox->setChecked(autoUpdate());
            connect(autoBox, &QCheckBox::toggled, this, [](bool on) { setAutoUpdate(on); });
            grid->addWidget(autoBox, 4, 0, 1, 3);
        }

        auto refresh = [this, id, state, bar, action, removeButton] {
            const bool downloading = busy(id);
            bar->setVisible(downloading);
            removeButton->setVisible(!downloading && ownCopy(id));
            action->setEnabled(!downloading);
            if (downloading)
            {
                const double p = progress(id);
                bar->setRange(0, p < 0 ? 0 : 1000); /* busy indicator until the size is known */
                if (p >= 0)
                    bar->setValue(int(p * 1000));
                const QString text = progressText(id);
                state->setText(text.isEmpty() ? qtr("Downloading…") : qtr("Downloading… %1").arg(text));
                action->setText(qtr("Downloading"));
                return;
            }
            if (installed(id))
            {
                const QString where = ownCopy(id) ? qtr("downloaded by Terrikon")
                                                  : QDir::toNativeSeparators(QFileInfo(findTool(id)).absolutePath());
                state->setText(qtr("Version %1, %2").arg(version(id), where));
                action->setText(ownCopy(id) ? qtr("Update") : qtr("Download a copy"));
            }
            else
            {
                state->setText(qtr("Not installed"));
                action->setText(qtr("Download"));
            }
        };
        refresh();
        connect(action, &QPushButton::clicked, this, [this, id] { install(id); });
        connect(removeButton, &QPushButton::clicked, this, [this, id] { remove(id); });
        connect(this, &Components::changed, card, [id, refresh](const QString &changedId) {
            if (changedId == id)
                refresh();
        });
        connect(this, &Components::progressChanged, card, [id, refresh](const QString &changedId) {
            if (changedId == id)
                refresh();
        });
        connect(this, &Components::failed, card, [id, state](const QString &failedId, const QString &error) {
            if (failedId == id)
                state->setText(qtr("Could not download: %1").arg(error));
        });
        column->addWidget(card);
    }

    auto buttons = new QHBoxLayout;
    auto openFolder = new QPushButton(qtr("Open the folder"));
    connect(openFolder, &QPushButton::clicked, this, [this] {
        QDir().mkpath(toolsDir());
        QDesktopServices::openUrl(QUrl::fromLocalFile(toolsDir()));
    });
    auto close = new QPushButton(qtr("Close"));
    connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    buttons->addWidget(openFolder);
    buttons->addStretch();
    buttons->addWidget(close);
    column->addLayout(buttons);

    m_dialog = dialog;
    dialog->show();
}

}
