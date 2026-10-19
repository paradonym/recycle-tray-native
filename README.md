# recycle-tray-native

## human / AI Todos
- SmartScreen signing
- cleanmgr.exe context menu option
- msix packaging
- Microsoft Windows Store publication

once was [isc30/recycle-tray](https://github.com/isc30/recycle-tray) but in C++

Build with:

```
cl /EHsc /std:c++17 RecycleTray.cpp /link user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib advapi32.lib
```
