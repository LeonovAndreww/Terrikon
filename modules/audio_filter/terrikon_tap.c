/*****************************************************************************
 * terrikon_tap.c: listens to the audio for the Terrikon visualizer
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

/* The audio passes through untouched. Every WINDOW_HOP samples the filter
 * takes the last WINDOW_SIZE of them, mixed to mono, and writes their
 * spectrum in log spaced bands, their shape and their level to the tap that
 * the interface published. Without an interface listening the filter does
 * not open. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>
#include <string.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_aout.h>
#include <vlc_filter.h>

#include "terrikon_tap.h"

#define WINDOW_SIZE 2048
#define WINDOW_HOP  1024

#define LOW_HZ  40.f
#define HIGH_HZ 16000.f

/* Band levels in decibels below full scale, mapped to 0..1 */
#define FLOOR_DB (-66.f)
#define TOP_DB   (-6.f)

typedef struct
{
    struct terrikon_tap *tap;
    unsigned channels;
    unsigned rate;

    float history[WINDOW_SIZE];         /* a ring of mono samples */
    unsigned pos;                       /* where the next one goes */
    unsigned filled;
    unsigned since_frame;

    float window[WINDOW_SIZE];
    float cos_table[WINDOW_SIZE / 2];
    float sin_table[WINDOW_SIZE / 2];
    unsigned edges[TERRIKON_TAP_BANDS + 1];

    float re[WINDOW_SIZE];
    float im[WINDOW_SIZE];
    struct terrikon_tap_frame frame;
} filter_sys_t;

static int  Open(vlc_object_t *);
static void Close(filter_t *);

vlc_module_begin()
    set_shortname(N_("Terrikon tap"))
    set_description(N_("Listens to the audio for the Terrikon visualizer"))
    set_subcategory(SUBCAT_AUDIO_AFILTER)
    set_capability("audio filter", 0)
    set_callback(Open)
vlc_module_end()

/* In place radix 2 transform of re/im, WINDOW_SIZE long */
static void Transform(filter_sys_t *sys)
{
    float *re = sys->re, *im = sys->im;
    const unsigned n = WINDOW_SIZE;

    for (unsigned i = 1, j = 0; i < n; i++)
    {
        unsigned bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
        {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }

    for (unsigned len = 2; len <= n; len <<= 1)
    {
        const unsigned half = len >> 1;
        const unsigned step = n / len;
        for (unsigned i = 0; i < n; i += len)
            for (unsigned k = 0; k < half; k++)
            {
                const float wr = sys->cos_table[k * step];
                const float wi = -sys->sin_table[k * step];
                const unsigned a = i + k, b = a + half;
                const float xr = re[b] * wr - im[b] * wi;
                const float xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
            }
    }
}

static void Publish(filter_sys_t *sys, vlc_tick_t ts)
{
    struct terrikon_tap_frame *frame = &sys->frame;

    /* The window, oldest sample first */
    for (unsigned i = 0; i < WINDOW_SIZE; i++)
    {
        const float s = sys->history[(sys->pos + i) % WINDOW_SIZE];
        sys->re[i] = s * sys->window[i];
        sys->im[i] = 0.f;
    }
    Transform(sys);

    /* A full scale sine comes out at 1 with a Hann window */
    const float scale = 4.f / WINDOW_SIZE;
    for (unsigned b = 0; b < TERRIKON_TAP_BANDS; b++)
    {
        float peak = 0.f;
        for (unsigned k = sys->edges[b]; k < sys->edges[b + 1]; k++)
        {
            const float m = sys->re[k] * sys->re[k] + sys->im[k] * sys->im[k];
            if (m > peak)
                peak = m;
        }
        const float db = 10.f * log10f(peak * scale * scale + 1e-12f);
        float v = (db - FLOOR_DB) / (TOP_DB - FLOOR_DB);
        frame->bands[b] = v < 0.f ? 0.f : v > 1.f ? 1.f : v;
    }

    /* The newest WINDOW_HOP samples, averaged down to the wave length */
    const unsigned per = WINDOW_HOP / TERRIKON_TAP_WAVE;
    float energy = 0.f;
    for (unsigned w = 0; w < TERRIKON_TAP_WAVE; w++)
    {
        float sum = 0.f;
        for (unsigned i = 0; i < per; i++)
        {
            const unsigned at = WINDOW_SIZE - WINDOW_HOP + w * per + i;
            const float s = sys->history[(sys->pos + at) % WINDOW_SIZE];
            sum += s;
            energy += s * s;
        }
        frame->wave[w] = sum / per;
    }
    const float rms = sqrtf(energy / WINDOW_HOP);
    frame->level = rms * 2.f > 1.f ? 1.f : rms * 2.f;
    frame->ts = ts;

    struct terrikon_tap *tap = sys->tap;
    vlc_mutex_lock(&tap->lock);
    tap->ring[tap->head] = *frame;
    tap->head = (tap->head + 1) % TERRIKON_TAP_RING;
    if (tap->count < TERRIKON_TAP_RING)
        tap->count++;
    vlc_mutex_unlock(&tap->lock);
}

static block_t *Process(filter_t *filter, block_t *block)
{
    filter_sys_t *sys = filter->p_sys;
    const float *in = (const float *)block->p_buffer;
    const unsigned channels = sys->channels;
    const float gain = 1.f / channels;

    for (unsigned i = 0; i < block->i_nb_samples; i++)
    {
        float s = 0.f;
        for (unsigned c = 0; c < channels; c++)
            s += in[i * channels + c];

        sys->history[sys->pos] = s * gain;
        sys->pos = (sys->pos + 1) % WINDOW_SIZE;
        if (sys->filled < WINDOW_SIZE)
            sys->filled++;

        if (++sys->since_frame >= WINDOW_HOP && sys->filled == WINDOW_SIZE)
        {
            sys->since_frame = 0;
            vlc_tick_t ts = VLC_TICK_INVALID;
            if (block->i_pts != VLC_TICK_INVALID)
            {
                /* The centre of the window, half of it before this sample */
                ts = block->i_pts + vlc_tick_from_samples(i + 1, sys->rate)
                   - vlc_tick_from_samples(WINDOW_SIZE / 2, sys->rate);
            }
            Publish(sys, ts);
        }
    }
    return block;
}

static void Flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    sys->pos = sys->filled = sys->since_frame = 0;

    vlc_mutex_lock(&sys->tap->lock);
    sys->tap->count = 0;
    vlc_mutex_unlock(&sys->tap->lock);
}

static int Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;

    struct terrikon_tap *tap =
        var_GetAddress(vlc_object_instance(filter), TERRIKON_TAP_VAR);
    if (tap == NULL)
        return VLC_EGENERIC;

    if (filter->fmt_in.audio.i_format != VLC_CODEC_FL32
     || !AOUT_FMTS_IDENTICAL(&filter->fmt_in.audio, &filter->fmt_out.audio))
    {
        filter->fmt_in.audio.i_format = VLC_CODEC_FL32;
        aout_FormatPrepare(&filter->fmt_in.audio);
        filter->fmt_out.audio = filter->fmt_in.audio;
    }

    const unsigned channels = aout_FormatNbChannels(&filter->fmt_in.audio);
    const unsigned rate = filter->fmt_in.audio.i_rate;
    if (channels == 0 || rate == 0)
        return VLC_EGENERIC;

    filter_sys_t *sys = vlc_obj_calloc(obj, 1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    sys->tap = tap;
    sys->channels = channels;
    sys->rate = rate;

    for (unsigned i = 0; i < WINDOW_SIZE; i++)
        sys->window[i] = .5f - .5f * cosf(2.f * (float)M_PI * i / (WINDOW_SIZE - 1));
    for (unsigned k = 0; k < WINDOW_SIZE / 2; k++)
    {
        sys->cos_table[k] = cosf(2.f * (float)M_PI * k / WINDOW_SIZE);
        sys->sin_table[k] = sinf(2.f * (float)M_PI * k / WINDOW_SIZE);
    }

    /* Log spaced band edges in bins, each band at least one bin wide */
    const float high = fminf(HIGH_HZ, rate / 2.f);
    const float hz_per_bin = (float)rate / WINDOW_SIZE;
    for (unsigned b = 0; b <= TERRIKON_TAP_BANDS; b++)
    {
        const float hz = LOW_HZ * powf(high / LOW_HZ, (float)b / TERRIKON_TAP_BANDS);
        unsigned bin = (unsigned)lroundf(hz / hz_per_bin);
        if (bin < 1)
            bin = 1;
        if (b > 0 && bin <= sys->edges[b - 1])
            bin = sys->edges[b - 1] + 1;
        if (bin > WINDOW_SIZE / 2)
            bin = WINDOW_SIZE / 2;
        sys->edges[b] = bin;
    }

    static const struct vlc_filter_operations ops =
    {
        .filter_audio = Process, .flush = Flush, .close = Close,
    };
    filter->p_sys = sys;
    filter->ops = &ops;

    Flush(filter);
    return VLC_SUCCESS;
}

static void Close(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;

    vlc_mutex_lock(&sys->tap->lock);
    sys->tap->count = 0;
    vlc_mutex_unlock(&sys->tap->lock);
}
