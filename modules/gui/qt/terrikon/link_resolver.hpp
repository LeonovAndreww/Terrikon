/*****************************************************************************
 * link_resolver.hpp: web pages to playable streams, through yt-dlp
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
#ifndef TERRIKON_LINK_RESOLVER_HPP
#define TERRIKON_LINK_RESOLVER_HPP

#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>

class QObject;

namespace terrikon {

/* A web link that is a page rather than a stream (YouTube, TikTok,
 * Bilibili...): not a file of a known media or playlist type */
bool isWebPage(const QUrl &url);

struct ResolvedLink
{
    QString mrl;          /* the video (or the only) stream */
    QString title;
    QStringList options;  /* audio as an input-slave, http headers */
    QString error;        /* empty when resolved */
};

/* Asks yt-dlp (next to the player, in its "tools" folder, or in PATH) for
 * the streams of a page; done() runs on context's thread, unless context
 * is gone by then. */
void resolveLink(QObject *context, const QUrl &url,
                 const std::function<void(const ResolvedLink &)> &done);

}

#endif
