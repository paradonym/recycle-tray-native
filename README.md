# recycle-tray-native

## Human Text:
GH copilot seems to have stolen isc30/recycle-tray instead of accurately forking it like instructed. Following process:
- GH Copilot rewote code to C++ (I didn't want to rely on .NET)
- I had to do some minor fixes with the AI - like empty traybar icon and so on
this is why RecycleTray.cpp isn't the logic of isc30/recycle-tray
- Then I used an Innosetup install script to bake it into an actual installer so we can talk about an actual release to do in the future.
- low priviledge userspace installation, because there is no necessity to act as admin for all program features

Reason this has been done is that all similar projects, including isc30/recycle-tray seem to have stopped at around 2018, which is at least some too many years ago...
This will be abandoned too sooner or later, but is made in 2026 with AI on training data from god knows when....

## human / AI Todos
- SmartScreen signing
- cleanmgr.exe context menu option
- 

## AI Text: producer GitHub Copilot:
Native Win32 port of isc30/recycle-tray: a small tray application that monitors the Windows Recycle Bin and provides quick actions (open, empty). This version is written in native C++ and produces a single EXE — no .NET runtime required.

This repository contains:
- RecycleTray.cpp — native Win32/C++ implementation (uses Shell API + GDI+ to load PNG icons)

Notes / origin
- Original project: https://github.com/isc30/recycle-tray
  I ported the behavior from the original C#/.NET Core 3.1 WinForms app to a native Win32 application.
- Icons (light.png / dark.png) are not included here. You can reuse the original icons from the upstream repo (links below) or place your own icons next to the EXE.
  - Light icon: https://raw.githubusercontent.com/isc30/recycle-tray/master/src/light.png
  - Dark icon:  https://raw.githubusercontent.com/isc30/recycle-tray/master/src/dark.png

Build (Visual Studio / Developer Command Prompt)

1) Open "Developer Command Prompt for VS" or use an MSVC environment.
2) Put RecycleTray.cpp and the desired icons (light.png / dark.png) in the same folder.
3) Build with:

```
cl /EHsc /std:c++17 RecycleTray.cpp /link user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib advapi32.lib
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

