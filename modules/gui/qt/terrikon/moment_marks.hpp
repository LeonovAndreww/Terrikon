/*****************************************************************************
 * moment_marks.hpp: marked moments of the playing file
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
#ifndef TERRIKON_MOMENT_MARKS_HPP
#define TERRIKON_MOMENT_MARKS_HPP

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVector>

namespace terrikon {

/* Moments marked while watching (Alt+M), kept per file in the user data
 * folder. They show on the seek bar and the clip edges snap to them. Unlike
 * media library bookmarks they need neither the library nor a name. */
class MomentMarks : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList times READ times NOTIFY changed FINAL)
    Q_PROPERTY(int count READ count NOTIFY changed FINAL)

public:
    /* Two marks closer than this are the same moment */
    static constexpr double sameMoment = 1.;
    static constexpr double backGrace = 1.5;

    using QObject::QObject;

    /* Follows the playing file; anything but a local file has no marks */
    Q_INVOKABLE void loadUrl(const QUrl &url);

    /* Adds a mark at "seconds", or removes the one already there */
    Q_INVOKABLE void toggle(double seconds);
    Q_INVOKABLE void removeAt(int index);
    Q_INVOKABLE void clearAll();

    /* The mark within "tolerance" of "seconds" if any, else "seconds" */
    Q_INVOKABLE double snap(double seconds, double tolerance) const;
    /* The first mark after "seconds", or the last one before it, -1 if
     * none. Going back skips a mark passed less than backGrace ago, as a
     * "previous track" button does, or playback would stick to it. */
    Q_INVOKABLE double next(double seconds) const;
    Q_INVOKABLE double previous(double seconds) const;

    QVariantList times() const;
    int count() const { return m_times.size(); }

signals:
    void changed();

private:
    void save() const;
    void loadFile(const QString &file);

    QString m_file; /* where the marks of the current media are stored */
    QString m_wanted; /* the media asked for last */
    QVector<double> m_times; /* sorted */
};

}

#endif
