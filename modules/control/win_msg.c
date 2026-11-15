/*****************************************************************************
 * ntservice.c: Windows NT/2K/XP service interface
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_url.h> // FIXME: move URL generation to calling process
#include <vlc_input_item.h>

#include <windows.h>
#include <wctype.h>

struct intf_sys_t
{
    HWND window;
    HANDLE ready;
    vlc_thread_t thread;
};

/* The name of the mutex and of the window: one per installation, so that a
 * copy elsewhere (a test, a portable one) is a player of its own.
 * Must be same as in src/win32/specific.c */
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

/* Must be same as in src/win32/specific.c */
typedef struct
{
    int argc;
    int enqueue;
    char data[];
} vlc_ipc_data_t;

static void add_to_playlist(intf_thread_t *intf, const char *uri,
                            bool play_now, int options_count,
                            const char *const *options)
{
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);

    input_item_t *media = input_item_New(uri, NULL);
    if (!media)
        return;
    input_item_AddOptions(media, options_count, options, VLC_INPUT_OPTION_TRUSTED);

    vlc_playlist_Lock(playlist);
    vlc_playlist_AppendOne(playlist, media);
    if (play_now)
        vlc_playlist_Start(playlist);
    vlc_playlist_Unlock(playlist);
    input_item_Release(media);
}

/* Brings the player's main window to the front: the biggest visible window
 * of this process that no other owns (the picture in picture one when it
 * is all that is shown). */
struct front_search
{
    HWND best;
    LONG area;
};

static BOOL CALLBACK FindFrontWindow(HWND hwnd, LPARAM data)
{
    struct front_search *search = (struct front_search *)data;
    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || GetWindow(hwnd, GW_OWNER) != NULL
     || !IsWindowVisible(hwnd))
        return TRUE;
    RECT rect;
    if (!GetWindowRect(hwnd, &rect))
        return TRUE;
    LONG area = (rect.right - rect.left) * (rect.bottom - rect.top);
    if (area > search->area)
    {
        search->best = hwnd;
        search->area = area;
    }
    return TRUE;
}

static void BringToFront(void)
{
    struct front_search search = { NULL, 0 };
    EnumWindows(FindFrontWindow, (LPARAM)&search);
    if (search.best == NULL)
        return;
    /* asynchronous: the window is the interface thread's */
    if (IsIconic(search.best))
        ShowWindowAsync(search.best, SW_RESTORE);
    SetForegroundWindow(search.best);
}

static LRESULT CALLBACK WMCOPYWNDPROC(HWND hwnd, UINT uMsg,
                                      WPARAM wParam, LPARAM lParam)
{
    if( uMsg == WM_QUIT  )
    {
        PostQuitMessage( 0 );
    }
    else if( uMsg == WM_COPYDATA )
    {
        COPYDATASTRUCT *pwm_data = (COPYDATASTRUCT*)lParam;

        intf_thread_t *intf = (intf_thread_t *)(uintptr_t)
            GetWindowLongPtr( hwnd, GWLP_USERDATA );
        if( intf == NULL )
            return 0; /* XXX: is this even possible? */

        /* Another start of the player: it comes to the front, with the
         * files it was given if any */
        BringToFront();

        /* Add files to the playlist */
        if( pwm_data->lpData && pwm_data->cbData >= sizeof(vlc_ipc_data_t) )
        {
            char **ppsz_argv;
            vlc_ipc_data_t *p_data = (vlc_ipc_data_t *)pwm_data->lpData;
            /* any window of the session may send this: nothing is read
             * beyond the cbData bytes that came */
            const size_t i_size = pwm_data->cbData - sizeof(vlc_ipc_data_t);
            size_t i_data = 0;
            size_t i_len;
            int i_argc = p_data->argc, i_opt, i_options;

            if( i_argc <= 0 || (size_t)i_argc > i_size / (sizeof(size_t) + 1) )
                return 0;
            ppsz_argv = vlc_alloc( i_argc, sizeof(char *) );
            if( unlikely(ppsz_argv == NULL) )
                return 0;
            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                if( i_size - i_data < sizeof(size_t) )
                    break;
                memcpy( &i_len, &p_data->data[i_data], sizeof(size_t) );
                i_data += sizeof(size_t);
                /* a string with its end, all within the message */
                if( i_len == 0 || i_len > i_size - i_data
                 || p_data->data[i_data + i_len - 1] != '\0' )
                    break;
                ppsz_argv[i_opt] = &p_data->data[i_data];
                i_data += i_len;
            }
            if( i_opt < i_argc )
            {
                free( ppsz_argv );
                return 0;
            }

            for( i_opt = 0; i_opt < i_argc; i_opt++ )
            {
                i_options = 0;

                /* Count the input options */
                while( i_opt + i_options + 1 < i_argc &&
                        *ppsz_argv[ i_opt + i_options + 1 ] == ':' )
                {
                    i_options++;
                }

#warning URI conversion must be done in calling process instead!
                /* FIXME: This breaks relative paths if calling vlc.exe is
                 * started from a different working directory. */
                char *psz_URI = NULL;
                if( strstr( ppsz_argv[i_opt], "://" ) == NULL )
                    psz_URI = vlc_path2uri( ppsz_argv[i_opt], NULL );
                add_to_playlist(intf, (psz_URI != NULL) ? psz_URI : ppsz_argv[i_opt],
                                (i_opt == 0 && !p_data->enqueue), i_options,
                                (char const **)( i_options ? &ppsz_argv[i_opt+1] : NULL ));
                i_opt += i_options;
                free( psz_URI );
            }

            free( ppsz_argv );
        }
    }

    return DefWindowProc( hwnd, uMsg, wParam, lParam );
}

static void *HelperThread(void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t *sys = intf->p_sys;

    vlc_thread_set_name("vlc-ctrl-win");

    wchar_t name[64];
    ipc_name(name, ARRAY_SIZE(name));
    HWND ipcwindow =
        CreateWindow(L"STATIC",                      /* name of window class */
                  name,                              /* window title bar text */
                  0,                                         /* window style */
                  0,                                 /* default X coordinate */
                  0,                                 /* default Y coordinate */
                  0,                                         /* window width */
                  0,                                        /* window height */
                  NULL,                                  /* no parent window */
                  NULL,                            /* no menu in this window */
                  GetModuleHandle(NULL),  /* handle of this program instance */
                  NULL) ;                               /* sent to WM_CREATE */

    SetWindowLongPtr(ipcwindow, GWLP_WNDPROC, (LRESULT)WMCOPYWNDPROC);
    SetWindowLongPtr(ipcwindow, GWLP_USERDATA, (uintptr_t)data);

    sys->window = ipcwindow;
    /* Signal the creation of the thread and events queue */
    SetEvent(sys->ready);

    MSG message;
    while (GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return NULL;
}

static int Open(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    intf->p_sys = sys;

    /* Run the helper thread */
    sys->ready = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (vlc_clone(&sys->thread, HelperThread, intf))
    {
        free(sys);
        msg_Err(intf, "one instance mode DISABLED "
                 "(IPC helper thread couldn't be created)");
        return VLC_ENOMEM;
    }

    WaitForSingleObject(sys->ready, INFINITE);
    CloseHandle(sys->ready);

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    intf_sys_t *sys = intf->p_sys;

    SendMessage(sys->window, WM_QUIT, 0, 0);
    vlc_join(sys->thread, NULL);
    free(sys);
}

vlc_module_begin()
    set_shortname(N_("WinMsg"))
    set_description(N_("Windows messages interface"))
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_capability("interface", 0)
    set_callbacks(Open, Close)
vlc_module_end()
