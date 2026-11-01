# recycle-tray-native

## Mostly GitHub Copilot / Claude Code / AI Vibe Code clone

## human / AI Todos
- SmartScreen signing
- cleanmgr.exe context menu option
- msix packaging
- Microsoft Windows Store publication

once was [isc30/recycle-tray](https://github.com/isc30/recycle-tray) but in C++

Build with:

```
cl /EHsc /std:c++17 RecycleTray.cpp /link resource.res user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib advapi32.lib
```

The produced RecycleTray.exe is a native Windows executable and does not require the .NET runtime.

Usage
- Run the exe; if the Recycle Bin contains items the tray icon will be visible and show the item count in its tooltip.
- Left-click on the icon to open the Recycle Bin in Explorer.
- Right-click to open the context menu with "Open", "Empty Trash Bin", and "Exit".
- To start at login, place a shortcut to the exe in your Startup folder:
  `%APPDATA%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup`

License
- This repository is a port of isc30/recycle-tray. Please check the original repository for licensing information and attribution.
