; Inno Setup script for the Windows installer.
; Build:  ISCC /DAppVersion=0.1.0 installer\windows\UNIT808.iss
; (paths are relative to this file; expects a Release build in ..\..\build)

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#define Artefacts "..\..\build\UNIT808_artefacts\Release"

[Setup]
AppId={{9C2E7B14-3A6F-4D1E-8B5A-1A0B808C0001}
AppName=UNIT808
AppVersion={#AppVersion}
AppVerName=UNIT808 {#AppVersion}
AppPublisher=Transistor Labs
DefaultDirName={autopf}\Transistor Labs\UNIT808
DefaultGroupName=Transistor Labs
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=UNIT808-{#AppVersion}-Windows-Setup
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=UNIT808

[Types]
Name: "full";   Description: "VST3 plug-in and standalone app"
Name: "custom"; Description: "Choose components"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Ableton Live, Bitwig, Reaper, Cubase...)"; Types: full custom
Name: "app";  Description: "Standalone app";                                         Types: full custom

[Files]
Source: "{#Artefacts}\VST3\UNIT808.vst3\*"; DestDir: "{commoncf64}\VST3\UNIT808.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Artefacts}\Standalone\UNIT808.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\UNIT808"; Filename: "{app}\UNIT808.exe"; Components: app

[Messages]
FinishedLabel=UNIT808 is installed.%n%nRescan plug-ins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
