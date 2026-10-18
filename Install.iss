[Setup] 
AppName=Recycle Tray
AppVersion=1.0
DefaultDirName={userappdata}\Recycle Tray
DefaultGroupName=Recycle Tray
Compression=lzma
SolidCompression=yes
OutputBaseFilename=RecycleTraySetup
ArchitecturesAllowed=x64compatible
PrivilegesRequired=lowest

[Files]
Source: "RecycleTray.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "light.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "dark.png"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Recycle Tray"; Filename: "{app}\RecycleTray.exe"; IconFilename: "{app}\RecycleTray.exe"
Name: "{userdesktop}\Recycle Tray"; Filename: "{app}\RecycleTray.exe"; Tasks: desktopicon; IconFilename: "{app}\RecycleTray.exe"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop icon"; Flags: unchecked
Name: "autostart"; Description: "Start Recycle Tray with Windows"; Flags: unchecked

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "RecycleTray"; ValueType: string; ValueData: """{app}\RecycleTray.exe"""; Check: WizardIsTaskSelected('autostart')

[Run]
Filename: "{app}\RecycleTray.exe"; Description: "Launch Recycle Tray"; Flags: nowait postinstall skipifsilent
