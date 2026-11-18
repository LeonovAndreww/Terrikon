/*****************************************************************************
 * explorer_order.cpp: Terrikon, the order an explorer window shows a folder in
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "explorer_order.hpp"

#include <QDir>

#ifdef _WIN32
# include <windows.h>
# include <shlobj.h>
# include <shlguid.h>
# include <shobjidl.h>
# include <exdisp.h>
# include <servprov.h>
# include <shlwapi.h>
#endif

#ifndef SVGIO_FLAG_VIEWORDER
# define SVGIO_FLAG_VIEWORDER 0x80000000
#endif

namespace terrikon {

#ifdef _WIN32
namespace {

template <typename T> struct Com
{
    T *p = nullptr;
    ~Com() { if (p) p->Release(); }
    T **out() { return &p; }
    T *operator->() const { return p; }
};

QString cleanKey(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
}

/* The items of one window's view, if it shows that folder */
bool viewOrder(IDispatch *window, const QString &want, QStringList &out)
{
    Com<IServiceProvider> provider;
    Com<IShellBrowser> browser;
    Com<IShellView> view;
    Com<IFolderView> folderView;
    if (FAILED(window->QueryInterface(IID_IServiceProvider, (void **)provider.out()))
     || FAILED(provider->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, (void **)browser.out()))
     || FAILED(browser->QueryActiveShellView(view.out())) || !view.p
     || FAILED(view->QueryInterface(IID_IFolderView, (void **)folderView.out())))
        return false;

    Com<IPersistFolder2> persist;
    if (FAILED(folderView->GetFolder(IID_IPersistFolder2, (void **)persist.out())))
        return false;
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(persist->GetCurFolder(&pidl)) || !pidl)
        return false;
    wchar_t path[MAX_PATH];
    const bool same = SHGetPathFromIDListW(pidl, path)
                   && cleanKey(QString::fromWCharArray(path)) == want;
    CoTaskMemFree(pidl);
    if (!same)
        return false;

    Com<IShellFolder> folder;
    Com<IEnumIDList> items;
    if (FAILED(folderView->GetFolder(IID_IShellFolder, (void **)folder.out()))
     || FAILED(folderView->Items(SVGIO_ALLVIEW | SVGIO_FLAG_VIEWORDER, IID_IEnumIDList,
                                 (void **)items.out())) || !items.p)
        return false;
    PITEMID_CHILD child = nullptr;
    while (items->Next(1, &child, nullptr) == S_OK)
    {
        STRRET name = {};
        if (SUCCEEDED(folder->GetDisplayNameOf(child, SHGDN_FORPARSING, &name)))
        {
            /* any kind of answer, freed by the conversion */
            wchar_t *text = nullptr;
            if (SUCCEEDED(StrRetToStrW(&name, child, &text)) && text)
            {
                out << QDir::fromNativeSeparators(QString::fromWCharArray(text));
                CoTaskMemFree(text);
            }
        }
        CoTaskMemFree(child);
    }
    return true;
}

}

QStringList explorerOrder(const QString &folder)
{
    QStringList out;
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Com<IShellWindows> windows;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL,
                                   IID_IShellWindows, (void **)windows.out())))
    {
        const QString want = cleanKey(folder);
        long count = 0;
        windows->get_Count(&count);
        for (long i = 0; i < count; ++i)
        {
            VARIANT index;
            VariantInit(&index);
            index.vt = VT_I4;
            index.lVal = i;
            Com<IDispatch> window;
            if (windows->Item(index, window.out()) != S_OK || !window.p)
                continue;
            if (viewOrder(window.p, want, out))
                break;
        }
    }

    if (SUCCEEDED(init))
        CoUninitialize();
    return out;
}
#else
QStringList explorerOrder(const QString &)
{
    return {};
}
#endif

}
