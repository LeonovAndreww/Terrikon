/*****************************************************************************
 * lyrics_finder.hpp: Terrikon, lyrics for songs without them
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
#ifndef TERRIKON_LYRICS_FINDER_HPP
#define TERRIKON_LYRICS_FINDER_HPP

#include <QObject>
#include <QString>
#include <QByteArray>

#include "qt.hpp"

namespace terrikon {

/* Lyrics for the song playing when its file has none: a .lrc file next to
 * it or one found before are read by themselves; asked for, they are looked
 * for on LRCLIB (lrclib.net, free, lines with their times) and kept. They
 * show in the player's lyrics view, following the song. */
class LyricsFinder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged FINAL)

public:
    enum State { Idle, Searching, NotFound, Failed, Found };
    Q_ENUM(State)

    LyricsFinder(qt_intf_t *intf, QObject *parent = nullptr);

    int state() const { return m_state; }

    /* Looks for the lyrics of the song playing on the web: shows the
     * best one, or with all, gathers every version for choosing */
    Q_INVOKABLE void find(bool all = false);
    /* Another version found, a file of one's own, another search */
    Q_INVOKABLE void showMenu();
    /* An .lrc (or plain text) file of one's own for the song */
    Q_INVOKABLE void loadFile();

    /* LRC text as the player keeps lyrics: "ms\x1Ftext" parts apart by
     * \x1E; without times, the lines in order, none current */
    static QByteArray fromLrc(const QString &lrc);

signals:
    void stateChanged();
    void candidatesChanged();
    /* found when asked for all: the menu to choose from, again */
    void choicesReady();

private:
    struct Song { QString url, artist, title, album, key; int seconds = 0; };
    struct Candidate { QString label, lrc; double score = 0; };
    void choose(int index);
    void use(const QString &lrc);
    QList<Candidate> m_candidates;
    Song current() const;
    void lookLocally();
    void setState(State state);
    void apply(const QString &url, const QByteArray &raw);

    qt_intf_t *m_intf;
    State m_state = Idle;
    QString m_lookedAt;
    /* which playing an answer belongs to: the same song played again after
     * another is another playing, a late answer of the first is dropped */
    unsigned m_playing = 0;
};

}

#endif
