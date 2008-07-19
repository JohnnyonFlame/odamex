; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=Odamex
AppVerName=Odamex 0.4
AppPublisher=Odamex Dev Team
AppPublisherURL=http://odamex.net
AppSupportURL=http://odamex.net
AppUpdatesURL=http://odamex.net
DefaultDirName={pf}\odamex
DefaultGroupName=Odamex
AllowNoIcons=yes
LicenseFile=..\..\LICENSE
;InfoBeforeFile=..\..\CHANGES
OutputBaseFilename=odamex-0.4-win32
Compression=zip
SolidCompression=no

[Languages]
Name: english; MessagesFile: compiler:Default.isl
Name: french; MessagesFile: compiler:Languages\French.isl
Name: german; MessagesFile: compiler:Languages\German.isl
Name: spanish; MessagesFile: compiler:Languages\Spanish.isl

[Tasks]
Name: desktopicon; Description: {cm:CreateDesktopIcon}; GroupDescription: {cm:AdditionalIcons}; Flags: unchecked

[Types]
Name: full; Description: Full installation
Name: compact; Description: Client-only installation
Name: custom; Description: Custom installation; Flags: iscustom

[Components]
Name: base; Description: Base data; Types: full compact custom; Flags: fixed
Name: client; Description: Odamex Client; Types: full compact custom
Name: server; Description: Odamex Server; Types: full
Name: launcher; Description: Odalaunch (Game Launcher); Types: full compact custom
Name: libs; Description: Libraries (SDL, SDL_Mixer); Types: full compact


[Files]
Source: ..\..\odamex.exe; DestDir: {app}; Flags: ignoreversion; Components: client
Source: ..\..\odasrv.exe; DestDir: {app}; Flags: ignoreversion; Components: server
Source: ..\..\odasrv.cfg; DestDir: {app}; Flags: ignoreversion; Components: server
Source: ..\..\odalaunch.exe; DestDir: {app}; Flags: ignoreversion; Components: launcher
Source: ..\..\odamex.wad; DestDir: {app}; Flags: ignoreversion; Components: client server
Source: ..\..\mingwm10.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\SDL.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\SDL_mixer.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\ogg.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\smpeg.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\vorbis.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\vorbisfile.dll; DestDir: {app}; Flags: ignoreversion; Components: libs
Source: ..\..\CHANGELOG; DestDir: {app}; Flags: ignoreversion; Components: base
Source: ..\..\LICENSE; DestDir: {app}; Flags: ignoreversion; Components: base
Source: ..\..\MAINTAINERS; DestDir: {app}; Flags: ignoreversion; Components: base

; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[INI]
Filename: {app}\Odamex Website.url; Section: InternetShortcut; Key: URL; String: http://odamex.net
Filename: {app}\Releases Changelog.url; Section: InternetShortcut; Key: URL; String: http://odamex.net/wiki/Releases

[Icons]
Name: {group}\Odamex Client; Filename: {app}\odamex.exe; WorkingDir: {app}
Name: {group}\Odalaunch; Filename: {app}\odalaunch.exe; WorkingDir: {app}
Name: {group}\Odamex Server; Filename: {app}\odasrv.exe; WorkingDir: {app}
Name: {group}\{cm:ProgramOnTheWeb,Odamex}; Filename: {app}\Odamex Website.url
Name: {group}\Releases Changelog; Filename: {app}\Releases Changelog.url
Name: {group}\{cm:UninstallProgram,Odamex}; Filename: {uninstallexe}
Name: {userdesktop}\Odamex Client; Filename: {app}\odamex.exe; Tasks: desktopicon; WorkingDir: {app}

[Run]
Filename: {app}\odalaunch.exe; Description: {cm:LaunchProgram,Odalaunch}; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: files; Name: {app}\Odamex Website.url
Type: files; Name: {app}\Releases Changelog.url
Type: files; Name: {app}\odamex.out
