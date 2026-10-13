; Inno Setup script for Tabless (lowkey in 0.8, heap until 0.8.0) — builds a
; Windows installer around the portable windeployqt bundle produced by the
; CMake `portable` target.
;
; Invoked from CI (see .github/workflows/release.yml) with:
;   ISCC /DAppVersion=<tag> /DBundleDir=<abs path to build\heap-portable> \
;        /F<output basename> installer\tabless.iss
;
; AppVersion / BundleDir are required defines; sensible fallbacks let the
; script also be opened directly in the Inno Setup IDE for local testing.

#ifndef AppVersion
  #define AppVersion "0.0.0-dev"
#endif
#ifndef BundleDir
  #define BundleDir "..\build\heap-portable"
#endif

; A channel build (APP-309, /DChannel=feature or unstable) installs beside the
; user's own Tabless as a program of its own: its own AppId, folder, name and
; uninstaller, and none of the shared registrations below (URL schemes, login
; item, the old installs' leftovers), which its uninstall would otherwise take
; away from the real Tabless.
#ifdef Channel
  #define AppTitle "Tabless " + Channel
  #if Channel == "feature"
    #define AppIdValue "{{9B3E7C41-5A2D-4F8E-B6C1-3D7A9E2F4B18}"
  #else
    #define AppIdValue "{{C4A81F6E-2B9D-4E37-8F5A-6D1E3B7C9A25}"
  #endif
#else
  #define AppTitle "Tabless"
  #define AppIdValue "{{6F4C9E2A-3B7D-4E1F-9A6C-0D2B1E8F5A44}"
#endif

[Setup]
; The same AppId heap and lowkey used: installing Tabless over lowkey 0.8 or
; heap 0.7 is an upgrade of the same program (one entry in Apps & features,
; one uninstaller), not a second app beside it (APP-280, APP-283).
AppId={#AppIdValue}
AppName={#AppTitle}
AppVersion={#AppVersion}
AppPublisher=Tabless
; A new install goes to …\Tabless. An upgrade keeps the previous install's
; folder (lowkey's or heap's), so nothing that points into it breaks.
DefaultDirName={autopf}\{#AppTitle}
; Always ask where to install, also on an upgrade (prefilled with the folder
; of the previous install).
DisableDirPage=no
; "Install for all users" (Program Files, needs admin) stays the default;
; "only for me" installs anywhere without elevation. The in-app updater
; (APP-125) passes /ALLUSERS or /CURRENTUSER to stay in the mode the
; previous install used.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
DefaultGroupName={#AppTitle}
DisableProgramGroupPage=yes
SetupIconFile=..\design\brand-export\tabless\tabless.ico
UninstallDisplayIcon={app}\tabless.exe
UninstallDisplayName={#AppTitle}
OutputDir={#SourcePath}\Output
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

#ifndef Channel
[InstallDelete]
; heap 0.7's and lowkey 0.8's shortcuts and files: the program is tabless.exe,
; and Tabless moves the autostart entry over itself. lowkey.exe stays, as a
; launcher for what still starts it (lowkey 0.8's updater restarting the
; program it updated, a pinned taskbar shortcut).
Type: filesandordirs; Name: "{autoprograms}\heap."
Type: files; Name: "{autoprograms}\heap..lnk"
Type: files; Name: "{autodesktop}\heap..lnk"
Type: files; Name: "{app}\heap.exe"
Type: filesandordirs; Name: "{autoprograms}\lowkey"
Type: files; Name: "{autodesktop}\lowkey.lnk"
Type: files; Name: "{app}\lowkey-cli.exe"
Type: files; Name: "{app}\lowkey-updater.exe"
Type: files; Name: "{app}\lowkey-portable.txt"
#endif

[Files]
; Recursively pack the entire portable bundle (tabless.exe + Qt runtime + QML,
; and the lowkey.exe launcher).
Source: "{#BundleDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
; The AppUserModelID is what Windows shows toasts under (APP-155); tabless.exe
; sets the same one on its process (NotificationCenter_win.cpp).
Name: "{group}\{#AppTitle}"; Filename: "{app}\tabless.exe"; AppUserModelID: "local.tabless.app"
Name: "{group}\Uninstall {#AppTitle}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppTitle}"; Filename: "{app}\tabless.exe"; AppUserModelID: "local.tabless.app"; Tasks: desktopicon

#ifndef Channel
[Registry]
; tabless:// — toast buttons start Tabless with a tabless://notify URI (APP-155).
; Tabless also registers it per user at startup; this covers the first toast.
Root: HKA; Subkey: "Software\Classes\tabless"; ValueType: string; ValueName: ""; ValueData: "URL:tabless"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\tabless"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\tabless\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\tabless.exe"" ""%1"""
; lowkey:// and heap:// — a toast lowkey 0.8 or heap 0.7 showed may still sit
; in the Action Center.
Root: HKA; Subkey: "Software\Classes\lowkey"; ValueType: string; ValueName: ""; ValueData: "URL:lowkey"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\lowkey"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\lowkey\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\tabless.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\heap"; ValueType: string; ValueName: ""; ValueData: "URL:heap"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\heap"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\heap\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\tabless.exe"" ""%1"""
; Start at login (APP-154) is written by Tabless itself (it adopts lowkey's or
; heap's entry on its first start); uninstalling removes any of them.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "tabless"; Flags: uninsdeletevalue dontcreatekey
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "lowkey"; Flags: uninsdeletevalue dontcreatekey
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: none; ValueName: "heap"; Flags: uninsdeletevalue dontcreatekey
#endif

[Run]
Filename: "{app}\tabless.exe"; Description: "Launch {#AppTitle}"; Flags: nowait postinstall skipifsilent
