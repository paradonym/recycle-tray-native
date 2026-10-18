// RecycleTray.cpp
// Native Win32 tray app that shows the Recycle Bin status and allows opening/emptying it.
// Build (Developer Command Prompt / Visual Studio):
// cl /EHsc /std:c++17 RecycleTray.cpp /link user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <strsafe.h>
#include <tchar.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Gdiplus;

const UINT WM_TRAYICON = WM_USER + 1;
const UINT ID_TRAY_TIMER = 1001;
const UINT ID_MENU_OPEN = 2001;
const UINT ID_MENU_EMPTY = 2002;
const UINT ID_MENU_EXIT = 2003;

HICON LoadIconFromPng(const WCHAR* filename)
{
    Bitmap* bmp = Bitmap::FromFile(filename);
    if (!bmp) return NULL;
    HICON hIcon = NULL;
    if (bmp->GetHICON(&hIcon) != Ok) {
        hIcon = NULL;
    }
    delete bmp;
    return hIcon;
}

bool IsLightTheme()
{
    DWORD value = 0;
    HKEY hKey;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0, KEY_READ, &hKey);
    if (res == ERROR_SUCCESS) {
        DWORD data = 0;
        DWORD size = sizeof(data);
        res = RegQueryValueExW(hKey, L"SystemUsesLightTheme", NULL, NULL, (LPBYTE)&data, &size);
        if (res == ERROR_SUCCESS) value = data;
        RegCloseKey(hKey);
    }
    return value == 1;
}

void UpdateTrayIcon(HWND hwnd, HICON hIcon, bool visible, ULONGLONG itemCount)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = hIcon;
    WCHAR tip[128];
    if (visible) {
        StringCchPrintfW(tip, ARRAYSIZE(tip), L"%llu item(s) in the Recycle Bin", itemCount);
    } else {
        StringCchCopyW(tip, ARRAYSIZE(tip), L"Recycle Tray");
        nid.hIcon = NULL;
        nid.uFlags = NIF_MESSAGE | NIF_TIP; // hide icon by removing icon field when not visible
    }
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), tip);

    // We add once, then modify/delete on exit.
    // Try NIM_ADD first if not existing, otherwise NIM_MODIFY
    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

ULONGLONG QueryRecycleBinItemCount()
{
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    if (SUCCEEDED(SHQueryRecycleBinW(NULL, &info))) {
        // SHQUERYRBINFO has member i64NumItems (may be 64-bit count in modern headers)
        // To be safe, interpret as ULONGLONG via cast.
#ifdef _WIN64
        return (ULONGLONG)info.i64NumItems;
#else
        // some older headers expose i64NumItems as DWORD; attempt to access generically
        return (ULONGLONG)info.i64NumItems;
#endif
    }
    return 0;
}

void EmptyRecycleBinNoConfirm()
{
    // SHEmptyRecycleBinW from ShellAPI, pass SHERB_NOCONFIRMATION (0x1)
    SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION);
}

void OpenRecycleBinInExplorer()
{
    ShellExecuteW(NULL, L"open", L"explorer.exe", L"shell:RecycleBinFolder", NULL, SW_SHOWNORMAL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HICON hIcon = NULL;
    switch (msg)
    {
    case WM_CREATE:
        // start timer to check recycle bin every 5s
        SetTimer(hwnd, ID_TRAY_TIMER, 5000, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == ID_TRAY_TIMER) {
            ULONGLONG items = QueryRecycleBinItemCount();
            if (items > 0) {
                // Make sure icon is visible
                UpdateTrayIcon(hwnd, hIcon, true, items);
            } else {
                // Hide icon by setting visible false
                UpdateTrayIcon(hwnd, hIcon, false, items);
            }
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_OPEN, L"Open");
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_EMPTY, L"Empty Trash Bin");
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_EXIT, L"Exit");
            // Set foreground window so menu disappears correctly
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        } else if (lParam == WM_LBUTTONUP) {
            OpenRecycleBinInExplorer();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_MENU_OPEN:
            OpenRecycleBinInExplorer();
            break;
        case ID_MENU_EMPTY:
            EmptyRecycleBinNoConfirm();
            // immediately update
            {
                ULONGLONG items = QueryRecycleBinItemCount();
                UpdateTrayIcon(hwnd, hIcon, items > 0, items);
            }
            break;
        case ID_MENU_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TRAY_TIMER);
        // remove tray icon
        {
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid);
            nid.hWnd = hwnd;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
        }
        if (hIcon) DestroyIcon(hIcon);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    // Init GDI+
    GdiplusStartupInput gsi;
    ULONG_PTR gdiToken;
    GdiplusStartup(&gdiToken, &gsi, NULL);

    // choose icon based on registry theme
    bool light = IsLightTheme();
    const WCHAR* iconFile = light ? L"light.png" : L"dark.png";
    HICON hIcon = LoadIconFromPng(iconFile);
    if (!hIcon) {
        // fallback to system tray icon (cast MAKEINTRESOURCE to wide pointer)
        hIcon = LoadIconW(NULL, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    }

    // Register window class
    const wchar_t CLASS_NAME[] = L"RecycleTrayHiddenWindow";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    // Create hidden window
    HWND hwnd = CreateWindowExW(0, CLASS_NAME, L"RecycleTray", 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
    if (!hwnd) {
        if (hIcon) DestroyIcon(hIcon);
        GdiplusShutdown(gdiToken);
        return 0;
    }

    // add tray icon if there are items now
    ULONGLONG items = QueryRecycleBinItemCount();
    if (items > 0) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIcon;
        StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"Recycle Tray");
        Shell_NotifyIconW(NIM_ADD, &nid);
    } else {
        // still create an (invisible) tray icon entry so later modifications succeed
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(nid);
        nid.hWnd = hwnd;
        nid.uID = 1;
        nid.uFlags = NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"Recycle Tray");
        Shell_NotifyIconW(NIM_ADD, &nid);
        // then hide by removing icon field
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }

    // store icon in window userdata via SetProp (so WndProc can access if needed)
    SetPropW(hwnd, L"TrayIcon", (HANDLE)hIcon);

    // show icon initially (WndProc will refresh on timer)
    SetTimer(hwnd, ID_TRAY_TIMER, 1000, NULL);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // cleanup
    RemovePropW(hwnd, L"TrayIcon");
    if (hIcon) DestroyIcon(hIcon);
    GdiplusShutdown(gdiToken);
    return (int)msg.wParam;
}
