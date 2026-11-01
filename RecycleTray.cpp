// RecycleTray.cpp
// Native Win32 tray app that shows the Recycle Bin status and allows opening/emptying it.
// Build (Developer Command Prompt / Visual Studio):
// cl /EHsc /std:c++17 RecycleTray.cpp /link user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib shlwapi.lib

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <strsafe.h>
#include <tchar.h>
#include <shlwapi.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;

const UINT WM_TRAYICON = WM_USER + 1;
const UINT ID_TRAY_TIMER = 1001;
const UINT ID_MENU_OPEN = 2001;
const UINT ID_MENU_EMPTY = 2002;
const UINT ID_MENU_EXIT = 2003;
const UINT ID_MENU_CLEANMGR = 2004;
const UINT ID_MENU_AUTOSTART = 2005;

const WCHAR* RUN_KEY_PATH = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const WCHAR* AUTOSTART_VALUE_NAME = L"RecycleTray";

HICON LoadIconFromPng(const WCHAR* filename)
{
    // Resolve the file relative to the EXE folder, not the current working directory
    // (with Autostart the working directory is not the program folder).
    WCHAR fullPath[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, fullPath, ARRAYSIZE(fullPath));
    if (len == 0 || len >= ARRAYSIZE(fullPath)) return NULL;
    WCHAR* lastSlash = wcsrchr(fullPath, L'\\');
    if (!lastSlash) return NULL;
    *(lastSlash + 1) = L'\0';
    if (FAILED(StringCchCatW(fullPath, ARRAYSIZE(fullPath), filename))) return NULL;

    Bitmap* src = Bitmap::FromFile(fullPath);
    if (!src) return NULL;
    if (src->GetLastStatus() != Ok) {
        delete src;
        return NULL;
    }
    // create a bitmap sized for the system small icon to avoid scaling issues
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    Bitmap* bmp = new Bitmap(cx, cy, PixelFormat32bppARGB);
    Graphics g(bmp);
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.Clear(Color(0, 0, 0, 0));
    g.DrawImage(src, Rect(0, 0, cx, cy), 0, 0, src->GetWidth(), src->GetHeight(), UnitPixel);
    HICON hIcon = NULL;
    if (bmp->GetHICON(&hIcon) != Ok) {
        hIcon = NULL;
    }
    delete bmp;
    delete src;
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

// Tray icons: one for a filled and one for an empty Recycle Bin (theme dependent)
HICON g_hIconFull = NULL;
HICON g_hIconEmpty = NULL;

void LoadThemeIcons()
{
    bool light = IsLightTheme();
    HICON full = LoadIconFromPng(light ? L"light.png" : L"dark.png");
    if (!full) {
        // fallback to the system application icon
        full = LoadIconW(NULL, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
    }
    HICON empty = LoadIconFromPng(light ? L"light_empty.png" : L"dark_empty.png");
    if (!empty) {
        empty = CopyIcon(full); // fallback: same icon as for a filled bin
    }
    HICON oldFull = g_hIconFull;
    HICON oldEmpty = g_hIconEmpty;
    g_hIconFull = full;
    g_hIconEmpty = empty;
    if (oldFull) DestroyIcon(oldFull);
    if (oldEmpty) DestroyIcon(oldEmpty);
}

// Adds the tray icon (if not yet present) and updates icon + tooltip.
// The icon is always visible: "empty" icon for an empty bin, "full" icon otherwise.
void UpdateTrayIcon(HWND hwnd, ULONGLONG itemCount, ULONGLONG totalBytes)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = (itemCount > 0) ? g_hIconFull : g_hIconEmpty;
    WCHAR tip[128];
    if (itemCount > 0) {
        WCHAR sizeStr[32];
        if (!StrFormatByteSizeW((LONGLONG)totalBytes, sizeStr, ARRAYSIZE(sizeStr))) {
            StringCchCopyW(sizeStr, ARRAYSIZE(sizeStr), L"?");
        }
        StringCchPrintfW(tip, ARRAYSIZE(tip), L"%llu item(s) in the Recycle Bin (%s)", itemCount, sizeStr);
    }
    else {
        StringCchCopyW(tip, ARRAYSIZE(tip), L"Recycle Bin is empty");
    }
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), tip);

    // NIM_ADD fails harmlessly if the icon already exists, then NIM_MODIFY updates it
    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// Returns number of items and total size in bytes of everything in the Recycle Bin.
void QueryRecycleBin(ULONGLONG& itemCount, ULONGLONG& totalBytes)
{
    itemCount = 0;
    totalBytes = 0;
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    if (SUCCEEDED(SHQueryRecycleBinW(NULL, &info))) {
        itemCount = (ULONGLONG)info.i64NumItems;
        totalBytes = (ULONGLONG)info.i64Size;
    }
}

void EmptyRecycleBinNoConfirm()
{
    // SHEmptyRecycleBinW from ShellAPI, pass SHERB_NOCONFIRMATION (0x1)
    SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION);
}

void RunDiskCleanupAsAdmin()
{
    // "runas" verb triggers the UAC elevation prompt
    ShellExecuteW(NULL, L"runas", L"cleanmgr.exe", NULL, NULL, SW_SHOWNORMAL);
}

// ---- Autostart (current user only: HKCU\\...\\Run) ----

bool GetQuotedExePath(WCHAR* out, size_t cch)
{
    WCHAR path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, ARRAYSIZE(path));
    if (len == 0 || len >= ARRAYSIZE(path)) return false;
    return SUCCEEDED(StringCchPrintfW(out, cch, L"\"%s\"", path));
}

bool IsAutostartEnabled()
{
    WCHAR expected[MAX_PATH + 4];
    if (!GetQuotedExePath(expected, ARRAYSIZE(expected))) return false;

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return false;
    WCHAR value[MAX_PATH + 4] = {};
    DWORD size = sizeof(value) - sizeof(WCHAR); // keep room for a terminating null
    DWORD type = 0;
    LONG res = RegQueryValueExW(hKey, AUTOSTART_VALUE_NAME, NULL, &type, (LPBYTE)value, &size);
    RegCloseKey(hKey);
    // Only counts as enabled if it points to this very EXE (an outdated path after moving the EXE does not count)
    return res == ERROR_SUCCESS && type == REG_SZ && _wcsicmp(value, expected) == 0;
}

void SetAutostart(bool enable)
{
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY_PATH, 0, NULL, 0,
        KEY_SET_VALUE, NULL, &hKey, NULL) != ERROR_SUCCESS)
        return;
    if (enable) {
        WCHAR value[MAX_PATH + 4];
        if (GetQuotedExePath(value, ARRAYSIZE(value))) {
            RegSetValueExW(hKey, AUTOSTART_VALUE_NAME, 0, REG_SZ,
                (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(WCHAR)));
        }
    }
    else {
        RegDeleteValueW(hKey, AUTOSTART_VALUE_NAME);
    }
    RegCloseKey(hKey);
}

// Draws a check mark or a cross into a bitmap sized for the menu check column.
HBITMAP CreateMenuMarkBitmap(bool check)
{
    int cx = GetSystemMetrics(SM_CXMENUCHECK);
    int cy = GetSystemMetrics(SM_CYMENUCHECK);
    HDC hdcScreen = GetDC(NULL);
    HDC hdc = CreateCompatibleDC(hdcScreen);
    HBITMAP bmp = CreateCompatibleBitmap(hdcScreen, cx, cy);
    HGDIOBJ oldBmp = SelectObject(hdc, bmp);

    RECT rc = { 0, 0, cx, cy };
    FillRect(hdc, &rc, GetSysColorBrush(COLOR_MENU));

    int thickness = (cx / 8 > 2) ? cx / 8 : 2;
    HPEN pen = CreatePen(PS_SOLID, thickness, check ? RGB(0, 150, 0) : RGB(200, 0, 0));
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    if (check) {
        MoveToEx(hdc, cx * 20 / 100, cy * 55 / 100, NULL);
        LineTo(hdc, cx * 42 / 100, cy * 75 / 100);
        LineTo(hdc, cx * 80 / 100, cy * 28 / 100);
    }
    else {
        int m = cx * 25 / 100;
        MoveToEx(hdc, m, m, NULL);
        LineTo(hdc, cx - m, cy - m);
        MoveToEx(hdc, cx - m, m, NULL);
        LineTo(hdc, m, cy - m);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
    SelectObject(hdc, oldBmp);
    DeleteDC(hdc);
    ReleaseDC(NULL, hdcScreen);
    return bmp;
}

void OpenRecycleBinInExplorer()
{
    ShellExecuteW(NULL, L"open", L"explorer.exe", L"shell:RecycleBinFolder", NULL, SW_SHOWNORMAL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        // start timer to check recycle bin every 5s
        SetTimer(hwnd, ID_TRAY_TIMER, 5000, NULL);
        return 0;

    case WM_TIMER:
        if (wParam == ID_TRAY_TIMER) {
            ULONGLONG items = 0, bytes = 0;
            QueryRecycleBin(items, bytes);
            UpdateTrayIcon(hwnd, items, bytes);
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_OPEN, L"Open");
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_CLEANMGR, L"Disk Cleanup (Administrator)");
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_EMPTY, L"Empty Trash Bin");
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            bool autostart = IsAutostartEnabled();
            InsertMenuW(hMenu, -1, MF_BYPOSITION | MF_STRING | (autostart ? MF_CHECKED : MF_UNCHECKED),
                ID_MENU_AUTOSTART, L"Autostart");
            HBITMAP bmpCheck = CreateMenuMarkBitmap(true);
            HBITMAP bmpCross = CreateMenuMarkBitmap(false);
            SetMenuItemBitmaps(hMenu, ID_MENU_AUTOSTART, MF_BYCOMMAND, bmpCross, bmpCheck);
            InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_MENU_EXIT, L"Exit");
            // Set foreground window so menu disappears correctly
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
            DeleteObject(bmpCheck);
            DeleteObject(bmpCross);
        }
        else if (lParam == WM_LBUTTONUP) {
            OpenRecycleBinInExplorer();
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_MENU_OPEN:
            OpenRecycleBinInExplorer();
            break;
        case ID_MENU_AUTOSTART:
            SetAutostart(!IsAutostartEnabled());
            break;
        case ID_MENU_CLEANMGR:
            RunDiskCleanupAsAdmin();
            break;
        case ID_MENU_EMPTY:
            EmptyRecycleBinNoConfirm();
            // immediately update
            {
                ULONGLONG items = 0, bytes = 0;
                QueryRecycleBin(items, bytes);
                UpdateTrayIcon(hwnd, items, bytes);
            }
            break;
        case ID_MENU_EXIT:
            DestroyWindow(hwnd); // WM_DESTROY removes the tray icon and quits
            break;
        }
        return 0;

    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        // theme or settings changed: reload icons according to current theme
    {
        LoadThemeIcons();
        // refresh tray icon immediately
        ULONGLONG items = 0, bytes = 0;
        QueryRecycleBin(items, bytes);
        UpdateTrayIcon(hwnd, items, bytes);
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
        // destroy icons
        if (g_hIconFull) { DestroyIcon(g_hIconFull); g_hIconFull = NULL; }
        if (g_hIconEmpty) { DestroyIcon(g_hIconEmpty); g_hIconEmpty = NULL; }
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

    // load icons (filled / empty) according to the current theme
    LoadThemeIcons();

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
        if (g_hIconFull) DestroyIcon(g_hIconFull);
        if (g_hIconEmpty) DestroyIcon(g_hIconEmpty);
        GdiplusShutdown(gdiToken);
        return 0;
    }

    // add tray icon with the state matching the current Recycle Bin content
    ULONGLONG items = 0, bytes = 0;
    QueryRecycleBin(items, bytes);
    UpdateTrayIcon(hwnd, items, bytes);

    // show icon initially (WndProc will refresh on timer)
    SetTimer(hwnd, ID_TRAY_TIMER, 1000, NULL);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // cleanup
    if (g_hIconFull) DestroyIcon(g_hIconFull);
    if (g_hIconEmpty) DestroyIcon(g_hIconEmpty);
    GdiplusShutdown(gdiToken);
    return (int)msg.wParam;
}