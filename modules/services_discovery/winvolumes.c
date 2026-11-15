/*****************************************************************************
 * Copyright © 2010 Rémi Denis-Courmont
 * Copyright © 2024 Prince Gupta <prince@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>
#include <vlc_threads.h>

#include <vlc_charset.h>
#include <windows.h>
#include <dbt.h>


// max drives allowed because of letter assignment (A-Z)
#define MAX_DRIVES 26

// name and window title used for observer window
static const char WindowClassName[] = "VLCWinVolumesDeviceObserver";

typedef struct volumes_observer_t
{
    // observer thread
    vlc_thread_t thread;

    // signal to end the observer thread
    HANDLE quit_signal;

    input_item_t *items[MAX_DRIVES];
} volumes_observer_t;


/**
 * @brief RemoteName names a network drive from its mapping, without asking
 *        its host
 * @param letter - drive letter
 * @return "share (\\host) (W:)", or "Network drive (W:)" (to be freed)
 */
static char *RemoteName( const char letter )
{
    /* "\Device\LanmanRedirector\;W:000000000001f2e3\host\share" */
    wchar_t drive[3] = L"A:", target[MAX_PATH + 1];
    drive[0] = letter;

    char *name = NULL;
    if (QueryDosDeviceW(drive, target, ARRAY_SIZE(target)) != 0)
    {
        const wchar_t *p = wcsstr(target, L"\\;");
        if (p != NULL)
            p = wcschr(p + 2, L'\\');
        if (p != NULL && p[1] != L'\0')
        {
            char *where = FromWide(p + 1);
            char *share = where != NULL ? strchr(where, '\\') : NULL;
            if (share != NULL && share[1] != '\0')
            {
                *share++ = '\0';
                if (asprintf(&name, "%s (\\\\%s) (%c:)", share, where,
                             letter) < 0)
                    name = NULL;
            }
            free(where);
        }
    }
    if (name == NULL
     && asprintf(&name, "%s (%c:)", _("Network drive"), letter) < 0)
        name = NULL;
    return name;
}


/**
 * @brief CreateInput
 * @param letter - drive letter assosited with drive
 * @return input item for the drive
 */
static input_item_t *CreateInput( const char letter )
{
    char mrl[12] = "file:///A:/";
    wchar_t path[4] = L"A:\\", label[MAX_PATH+1] = {0};

    path[0] = letter;
    mrl[8] = letter;

    const UINT type = GetDriveTypeW(path);
    char *name;

    if (type == DRIVE_REMOTE)
    {
        /* A network drive is not asked for its label: when its host does
         * not answer, the question stays for as long as Windows tries
         * (20 s and more). It is named from what it is mapped to, which is
         * known here: "share (\\host) (W:)", as the file explorer does. */
        name = RemoteName(letter);
    }
    else
    {
        /* The wide call: the label may be in any language */
        if (GetVolumeInformationW(path, label, ARRAY_SIZE(label),
                                  NULL, NULL, NULL, NULL, 0) == 0)
            return NULL;

        /* As the file explorer names them: "Label (E:)", and a drive
         * without a label is a local disk */
        char *utf8 = FromWide(label);
        if (asprintf(&name, "%s (%c:)",
                     (utf8 && *utf8) ? utf8 : _("Local disk"), letter) < 0)
            name = NULL;
        free(utf8);
    }
    if (name == NULL)
        return NULL;

    input_item_t *item;
    if (type == DRIVE_CDROM)
        item = input_item_NewDisc( mrl, name, INPUT_DURATION_INDEFINITE );
    else
        item = input_item_NewDirectory( mrl, name, ITEM_LOCAL );
    free(name);
    return item;
}


/**
 * @brief RegisterWindowClass registers window class for window creation
 * @param class_name
 * @param proc - window procedure associated with window class
 * @return ATOM (unique identifier) for the class on success otherwise 0
 */
static ATOM RegisterWindowClass( const char *class_name, WNDPROC proc )
{
    WNDCLASSEXA wcex =
    {
        .cbSize = sizeof(WNDCLASSEXA),
        .lpfnWndProc = proc,
        .hInstance = (HINSTANCE)GetModuleHandle(NULL),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW),
        .lpszClassName = class_name
    };

    return RegisterClassExA(&wcex);
}


/**
 * @brief StartWindow creates a hidden window, can be used to listen on OS events
 * @param class_name name and title for window
 * @param ctx will be passed as lpParam, can be used with CREATESTRUCT (see CreateWindowA docs)
 * @return HANDLE to the created window on success otherwise NULL
 */
static HWND StartWindow(const char *class_name, void *ctx)
{
    HWND window = CreateWindowA(class_name, class_name, WS_ICONIC,
                                0, 0, CW_USEDEFAULT, 0, NULL, NULL,
                                (HINSTANCE)GetModuleHandle(NULL), ctx);

    if (window == NULL)
        return NULL;

    ShowWindow(window, SW_HIDE);
    return window;
}


static int Add( services_discovery_t *p_sd, const uint32_t disk_mask )
{
    volumes_observer_t *p_sys = p_sd->p_sys;

    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        const uint32_t mask = 1 << i;
        if ( !(disk_mask & mask) )
            continue;

        /* no item: an empty reader, a drive with no disc in it. It is left
         * out, and the others are still listed. */
        input_item_t *item = CreateInput( 'A' + i );
        if ( !item )
            continue;

        if ( p_sys->items[i] )
        {
            services_discovery_RemoveItem( p_sd, p_sys->items[i] );
            input_item_Release( p_sys->items[i] );
        }

        p_sys->items[i] = item;

        // notify changes
        services_discovery_AddItem( p_sd, item );
    }

    return VLC_SUCCESS;
}


static void Remove(services_discovery_t *p_sd, const uint32_t disk_mask)
{
    volumes_observer_t *p_sys = p_sd->p_sys;

    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        const uint32_t mask = (1 << i);
        if ( !(disk_mask & mask) )
            continue;

        if ( p_sys->items[i] )
        {
            services_discovery_RemoveItem( p_sd, p_sys->items[i] );
            input_item_Release( p_sys->items[i] );
            p_sys->items[i] = NULL;
        }
    }
}


static int AddExistingDrives( services_discovery_t *p_sd )
{
    // find existing drives
    const DWORD drives = GetLogicalDrives();
    return Add( p_sd, drives );
}


static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        // store passed lParam as user data to use as context for handling other events
        CREATESTRUCT *c = (CREATESTRUCT *)lParam;
        SetWindowLongPtr( hWnd, GWLP_USERDATA, (LONG_PTR)c->lpCreateParams );
        return 0;
    }

    case WM_DEVICECHANGE:
    {
        if ((wParam != DBT_DEVICEARRIVAL) && (wParam != DBT_DEVICEREMOVECOMPLETE))
            break;

        DEV_BROADCAST_HDR *header = (DEV_BROADCAST_HDR *) lParam;
        if (header->dbch_devicetype != DBT_DEVTYP_VOLUME)
            break;

        // get associated user data for the context
        LONG_PTR user_data = GetWindowLongPtr( hWnd, GWLP_USERDATA );
        if ( user_data == 0 )
            break;

        services_discovery_t *p_sd = (services_discovery_t *)user_data;
        DEV_BROADCAST_VOLUME *node = (DEV_BROADCAST_VOLUME *)lParam;

        switch (wParam)
        {
        case DBT_DEVICEARRIVAL:
        {
            Add( p_sd, node->dbcv_unitmask );
            break;
        }
        case DBT_DEVICEREMOVECOMPLETE:
        {
            Remove( p_sd, node->dbcv_unitmask );
            break;
        }
        default:
            break;
        }

        break;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}


static void *Run(void *ctx)
{
    services_discovery_t *p_sd = ctx;
    volumes_observer_t *p_sys = p_sd->p_sys;

    // register window class
    ATOM registered_class = RegisterWindowClass( WindowClassName, WndProc );
    DWORD error = GetLastError();
    if ((registered_class == 0)
            && (error != ERROR_CLASS_ALREADY_EXISTS))
    {
        msg_Err( p_sd, "failed to register window class %lu", error );
        return NULL;
    }

    HWND window = StartWindow( WindowClassName, ctx );
    if ( !window )
    {
        msg_Err( p_sd, "failed to start window %lu", GetLastError() );
        return NULL;
    }

    /* Here and not in Open(): the module is opened with the media sources
     * locked, a lock the interface thread takes too, and a drive may take
     * its time to answer. After the window: no arrival is missed. */
    AddExistingDrives( p_sd );

    // window message loop
    for(;;)
    {
        // Wait for either a message or the quit event
        DWORD wait_result = MsgWaitForMultipleObjects(1, &p_sys->quit_signal,
                                                      FALSE, INFINITE, QS_ALLINPUT);

        if (wait_result == WAIT_OBJECT_0)
        {
            // The close event is signaled, break out of the loop
            break;
        }
        else if (wait_result == WAIT_OBJECT_0 + 1)
        {
            MSG msg;

            // There are messages in the queue, process them
            while (PeekMessage(&msg, window, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    break;
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
        else if (wait_result == WAIT_FAILED)
        {
            break;
        }
    }

    DestroyWindow( window );
    return NULL;
}


static void ReleaseItems( volumes_observer_t *p_sys )
{
    for (int i = 0; i < MAX_DRIVES; ++i)
    {
        if (p_sys->items[i])
            input_item_Release( p_sys->items[i] );
    }
}


// Module functions -
static int Open (vlc_object_t *obj)
{
    services_discovery_t *p_sd = (services_discovery_t *)obj;

    volumes_observer_t *p_sys = (volumes_observer_t *)calloc( 1, sizeof(volumes_observer_t) );
    if ( !p_sys )
        return VLC_ENOMEM;

    p_sd->p_sys = p_sys;
    p_sd->description = _("Lists volumes on host computer");

    p_sys->quit_signal = CreateEvent(NULL, TRUE, FALSE, NULL);
    if ( p_sys->quit_signal == NULL )
    {
        msg_Err( p_sd, "failed to create event, error: %lu", GetLastError());
        goto error;
    }

    // start the thread for window handling
    if ( vlc_clone( &p_sys->thread, Run, p_sd ) )
    {
        msg_Err( p_sd, "failed to start observer thread");
        goto error;
    }

    return VLC_SUCCESS;

error:
    if ( p_sys->quit_signal )
        CloseHandle( p_sys->quit_signal );

    ReleaseItems( p_sys );
    free( p_sys );
    return VLC_EGENERIC;
}


static void Close( vlc_object_t * obj )
{
    services_discovery_t *p_sd = (services_discovery_t *)obj;
    volumes_observer_t *p_sys = (volumes_observer_t *) p_sd->p_sys;

    // signal window thread to quit
    SetEvent( p_sys->quit_signal );

    vlc_join( p_sys->thread, NULL );

    // release resources
    CloseHandle( p_sys->quit_signal );
    ReleaseItems( p_sys );
    free( p_sys );
}


VLC_SD_PROBE_HELPER("volume", N_("Volumes"), SD_CAT_MYCOMPUTER)

/*
 * Module descriptor
 *
 * list volumes on Windows
 */
vlc_module_begin ()
    set_shortname (N_("Volumes"))
    set_description (N_("Lists volumes on host computer"))
    set_capability ("services_discovery", 0)
    set_callbacks(Open, Close)
    add_shortcut ("volume")

    VLC_SD_PROBE_SUBMODULE
vlc_module_end ()
