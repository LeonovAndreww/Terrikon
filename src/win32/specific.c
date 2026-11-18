/*****************************************************************************
 * specific.c: Win32 specific initialization
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef UNICODE
# define UNICODE
#endif
#include <vlc_common.h>
#include "../libvlc.h"
#include "../lib/libvlc_internal.h"

#include <winsock2.h>
#include <wctype.h>

static int system_InitWSA(int hi, int lo)
{
    WSADATA data;

    if (WSAStartup(MAKEWORD(hi, lo), &data) == 0)
    {
        if (LOBYTE(data.wVersion) == 2 && HIBYTE(data.wVersion) == 2)
            return 0;
        /* Winsock DLL is not usable */
        WSACleanup( );
    }
    return -1;
}

/**
 * Initializes Winsock.
 */
void system_Init(void)
{
    if (system_InitWSA(2, 2) && system_InitWSA(1, 1))
        fputs("Error: cannot initialize Winsocks\n", stderr);
}

/*****************************************************************************
 * system_Configure: check for system specific configuration options.
 *****************************************************************************/

/* The name of the mutex and of the window: one per installation, so that a
 * copy elsewhere (a test, a portable one) is a player of its own.
 * Must be same as in modules/control/win_msg.c */
static void ipc_name(wchar_t *name, size_t size)
{
    wchar_t exe[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, exe, MAX_PATH);
    uint32_t hash = 2166136261u; /* FNV-1a of the lower case path */
    for (DWORD i = 0; i < len; i++)
    {
        hash ^= (uint32_t)towlower(exe[i]);
        hash *= 16777619u;
    }
    _snwprintf(name, size, L"Terrikon ipc " TEXT(VERSION) L" %08x", hash);
    name[size - 1] = L'\0';
}

/* Must be same as in modules/control/win_msg.c */
typedef struct
{
    int argc;
    int enqueue;
    char data[];
} vlc_ipc_data_t;

void system_Configure( libvlc_int_t *p_this, int i_argc, const char *const ppsz_argv[] )
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    /* Every instance takes the files of the next ones: a player started on
     * its own, then a file opened from the explorer, gave two players, the
     * second without its media library (its base is the first's). Files
     * are passed as before, when asked to; a start without any is always
     * passed: it brings the player that runs to the front instead of
     * opening a second one. */
    const bool pass_files = var_InheritBool( p_this, "one-instance" )
     || ( var_InheritBool( p_this, "one-instance-when-started-from-file" )
       && var_InheritBool( p_this, "started-from-file" ) );
    /* Only the player: the preparser runs several processes on libvlc, and
     * each after the first, taken for a second start, went away (folders
     * could no longer be read: the media library dropped them) */
    if( !var_InheritBool( p_this, "terrikon-player" ) && !pass_files )
        return;
    {
        HANDLE hmutex;
        wchar_t name[64];
        ipc_name( name, ARRAY_SIZE(name) );

        /* Use a named mutex to check if another instance is already running */
        if( !( hmutex = CreateMutex( 0, TRUE, name ) ) )
        {
            /* Failed for some reason. Just ignore the option and go on as
             * normal. */
            msg_Err( p_this, "one instance mode DISABLED "
                     "(mutex couldn't be created)" );
            return;
        }

        if( GetLastError() != ERROR_ALREADY_EXISTS )
        {
            libvlc_InternalAddIntf( p_this, "win_msg,none" );
            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );
        }
        else if( !pass_files && i_argc > 0 )
        {
            /* files for a player of their own */
            CloseHandle( hmutex );
        }
        else
        {
            /* Another instance is running */

            HWND ipcwindow;

            /* Wait until the 1st instance is initialized */
            WaitForSingleObject( hmutex, INFINITE );

            /* Locate the window created by the IPC helper thread of the
             * 1st instance */
            if( !( ipcwindow = FindWindow( 0, name ) ) )
            {
                msg_Err( p_this, "one instance mode DISABLED "
                         "(couldn't find 1st instance of program)" );
                ReleaseMutex( hmutex );
                return;
            }

            /* That player may come to the front: this one was started by
             * the user, it may let another take the foreground. */
            DWORD first_pid;
            if( GetWindowThreadProcessId( ipcwindow, &first_pid ) )
                AllowSetForegroundWindow( first_pid );

            /* We assume that the remaining parameters are filenames
             * and their input options */
            if( i_argc == 0 )
            {
                /* nothing to open: only show the player */
                vlc_ipc_data_t show = { .argc = 0, .enqueue = 0 };
                COPYDATASTRUCT wm_data = {
                    .dwData = 0, .cbData = sizeof (show), .lpData = &show,
                };
                SendMessage( ipcwindow, WM_COPYDATA, 0, (LPARAM)&wm_data );
            }
            else
            {
                COPYDATASTRUCT wm_data;
                int i_opt;
                vlc_ipc_data_t *p_data;
                size_t i_data = sizeof (*p_data);

                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    i_data += sizeof (size_t);
                    i_data += strlen( ppsz_argv[ i_opt ] ) + 1;
                }

                p_data = malloc( i_data );
                if( unlikely(p_data == NULL) )
                    i_argc = 0; /* nothing is sent */
                else
                {
                    p_data->argc = i_argc;
                    p_data->enqueue = var_InheritBool( p_this, "playlist-enqueue" );
                }
                i_data = 0;
                for( i_opt = 0; i_opt < i_argc; i_opt++ )
                {
                    size_t i_len = strlen( ppsz_argv[ i_opt ] ) + 1;
                    memcpy( &p_data->data[i_data], &i_len, sizeof (size_t) );
                    i_data += sizeof (size_t);
                    memcpy( &p_data->data[i_data], ppsz_argv[ i_opt ], i_len );
                    i_data += i_len;
                }
                i_data += sizeof (*p_data);

                /* Send our playlist items to the 1st instance */
                if( p_data != NULL )
                {
                    wm_data.dwData = 0;
                    wm_data.cbData = i_data;
                    wm_data.lpData = p_data;
                    SendMessage( ipcwindow, WM_COPYDATA, 0, (LPARAM)&wm_data );
                    free( p_data );
                }
            }

            /* Initialization done.
             * Release the mutex to unblock other instances */
            ReleaseMutex( hmutex );

            /* Bye bye */
            system_End( );
            exit( 0 );
        }
    }
#endif
}

/**
 * Cleans up after system_Init() and system_Configure().
 */
void system_End(void)
{
    /* XXX: In theory, we should not call this if WSAStartup() failed. */
    WSACleanup();
}
