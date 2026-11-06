/*****************************************************************************
 * terrikon_tap.h: what the Terrikon tap filter hands to the interface
 *****************************************************************************
 * Copyright (C) 2026 the Terrikon authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef TERRIKON_TAP_H
#define TERRIKON_TAP_H

#include <vlc_common.h>
#include <vlc_threads.h>

/* The interface owns one of these for the whole run and publishes it on the
 * libvlc instance as the address variable TERRIKON_TAP_VAR. The filter fills
 * it with what it hears; the interface picks the frame that matches the
 * playing time, as the filter runs ahead of the speakers. */

#define TERRIKON_TAP_VAR   "terrikon-tap"
#define TERRIKON_TAP_BANDS 64
#define TERRIKON_TAP_WAVE  256
#define TERRIKON_TAP_RING  160

struct terrikon_tap_frame
{
    vlc_tick_t ts;                       /* stream time of the window centre */
    float bands[TERRIKON_TAP_BANDS];     /* 0..1, low to high, on a log scale */
    float wave[TERRIKON_TAP_WAVE];       /* -1..1, the latest samples, mono */
    float level;                         /* 0..1 */
};

struct terrikon_tap
{
    vlc_mutex_t lock;
    unsigned head;                       /* the next frame written */
    unsigned count;
    struct terrikon_tap_frame ring[TERRIKON_TAP_RING];
};

#endif
