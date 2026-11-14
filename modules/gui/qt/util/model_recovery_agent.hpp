/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
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

#ifndef MODEL_RECOVERY_AGENT_HPP
#define MODEL_RECOVERY_AGENT_HPP

#include <QLocale>
#include <QFile>
#include <QTimer>
#include <QSettings>
#include <QPointer>
#include <QFileInfo>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QAbstractItemModel>
#include <QLockFile>

#include <cstdio>

#include "qt.hpp"

#include "widgets/native/qvlcframe.hpp"

class ModelRecoveryAgent
{
    const QPointer<QSettings> m_settings;
    const QString m_key;
    QString m_recoveryFileName;
    QTimer m_timer;
    bool m_conditionDismissInitialDirtiness = false;
    std::unique_ptr<QLockFile> m_lockFile;
    std::unique_ptr<QMessageBox> m_question;

public:
    // NOTE: settings and model must outlive the instance of this class.
    template<class T>
    ModelRecoveryAgent(class QSettings *settings, const QString& modelIdentifier, T* model)
        : m_settings(settings), m_key(modelIdentifier + QStringLiteral("/RecoveryFilePath"))
    {
        assert(settings);
        assert(model);
        settings->sync();
        if (settings->contains(m_key))
        {
            assert(settings->value(m_key).typeId() == QMetaType::QString);
            QString recoveryFileName = settings->value(m_key).toString();
            if (!recoveryFileName.isEmpty())
            {
                m_recoveryFileName = std::move(recoveryFileName);
                {
                    m_lockFile = std::make_unique<QLockFile>(m_recoveryFileName + QStringLiteral(".lock"));
                    m_lockFile->setStaleLockTime(0); // if the process crashed, QLockFile considers the lock stale regardless of the lock time
                    if (!m_lockFile->tryLock()) // if the older instance is still alive, it would have the lock
                        throw std::exception(); // Older instance is managing the recovery, don't take over. We don't support recovering multiple models at the moment.
                }
                const QFileInfo fileInfo(m_recoveryFileName);
                if (fileInfo.size() > 0)
                {
                    m_question = std::make_unique<QMessageBox>();
                    /* Terrikon: in words and a date of the interface's language */
                    const QString what = modelIdentifier == QLatin1String("Playlist")
                                       ? qtr("the play queue") : modelIdentifier.toLower();
                    m_question->setText(qtr("Restore %1 as it was on %2?")
                                        .arg(what, QLocale().toString(fileInfo.lastModified(), QLocale::ShortFormat)));
                    m_question->setWindowTitle(qtr("The application closed abruptly."));
                    m_question->setIcon(QMessageBox::Question);
                    m_question->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                    m_question->setDefaultButton(QMessageBox::Yes);
                    m_question->setWindowModality(Qt::ApplicationModal);

                    QObject::connect(m_question.get(), &QDialog::finished, m_question.get(), [this, model]() {
                        if (m_question->standardButton(m_question->clickedButton()) == QMessageBox::Yes)
                        {
                            model->append(m_recoveryFileName);
                            m_conditionDismissInitialDirtiness = true;
                        }
                        m_question.release()->deleteLater();
                        startMonitoring(model);
                    });
                }
            }
        }

        if (m_recoveryFileName.isEmpty())
        {
            QTemporaryFile temporaryFile;
            temporaryFile.setAutoRemove(false);
            if (!temporaryFile.open())
                throw std::exception();
            m_recoveryFileName = temporaryFile.fileName();
            {
                assert(!m_lockFile);
                m_lockFile = std::make_unique<QLockFile>(m_recoveryFileName + QStringLiteral(".lock"));
                m_lockFile->setStaleLockTime(0); // if the process crashed, QLockFile considers the lock stale regardless of the lock time
                assert(!m_lockFile->isLocked()); // the file name is new here, it can not be locked before
                if (!m_lockFile->tryLock())
                    throw std::exception();
            }
            settings->setValue(m_key, m_recoveryFileName);
            settings->sync();
        }

        if (!m_question)
            startMonitoring(model);
    }

    void prompt(qt_intf_t *p_intf)
    {
        if (!m_question)
            return;
        QVLCDialog::setWindowTransientParent(m_question.get(), nullptr, p_intf);
        m_question->show();
    }

    ~ModelRecoveryAgent()
    {
        if (m_question)
            return;

        if (!m_recoveryFileName.isEmpty())
        {
            QFile::remove(m_recoveryFileName);
        }

        assert(m_settings);

        m_settings->sync();

        if (m_settings->value(m_key) == m_recoveryFileName)
        {
            m_settings->remove(m_key);
            m_settings->sync();
        }
    }

private:
    template<class T>
    void startMonitoring(T* model)
    {
        m_timer.setInterval(10000); // 10 seconds
        m_timer.setSingleShot(true);

        QObject::connect(&m_timer, &QTimer::timeout, model, [this, model]() {
            if (m_conditionDismissInitialDirtiness)
            {
                m_conditionDismissInitialDirtiness = false;
                return;
            }

            assert(!m_recoveryFileName.isEmpty());

            // QString all the way: a folder of the user may have any letters in its name
            const QString tmpFileName = m_recoveryFileName + QStringLiteral(".part");
            const QString oldFileName = m_recoveryFileName + QStringLiteral(".old");

            if (model->isEmpty())
            {
                QFile::remove(m_recoveryFileName);
                return;
            }

            if (model->serialize(tmpFileName) != VLC_SUCCESS)
                return;

            // the last good copy is kept until the new one is in place
            QFile::remove(oldFileName);
            const bool hadOld = QFile::rename(m_recoveryFileName, oldFileName);
            if (QFile::rename(tmpFileName, m_recoveryFileName))
            {
                QFile::remove(oldFileName);
                assert(m_settings);
                m_settings->sync();
                if (!m_settings->contains(m_key))
                {
                    m_settings->setValue(m_key, m_recoveryFileName);
                    m_settings->sync();
                }
                else if (m_settings->value(m_key) != m_recoveryFileName)
                {
                    QObject::disconnect(model, nullptr, &m_timer, nullptr);
                    m_timer.stop();
                }
            }
            else if (hadOld)
                QFile::rename(oldFileName, m_recoveryFileName);
        });

        QObject::connect(model, &T::itemsReset, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsAdded, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsMoved, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsRemoved, &m_timer, QOverload<>::of(&QTimer::start));
        QObject::connect(model, &T::itemsUpdated, &m_timer, QOverload<>::of(&QTimer::start));
    }
};

#endif // MODEL_RECOVERY_AGENT_HPP
