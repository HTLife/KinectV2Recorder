; -- Example3.iss --
; Same as Example1.iss, but creates some registry entries too.

; SEE THE DOCUMENTATION FOR DETAILS ON CREATING .ISS SCRIPT FILES!

#define MyAppName "KinectV2IMURecord"
#define MyAppExeName "KinectV2Recorder.exe"
#define MyAppIcoName "app.ico"
#define AppVer "1_1"
[Setup]
AppName={#MyAppName}
AppVersion={#AppVer}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename={#MyAppName}{#AppVer}
UninstallDisplayIcon={app}\uninstall.exe

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked


[Files]
Source: "..\x64\Release\KinectV2Recorder.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "boost_system-vc141-mt-1_64.dll"; DestDir: "{app}"
Source: "opencv_world340.dll"; DestDir: "{app}"
Source: "libcrypto-1_1-x64.dll"; DestDir: "{app}"
Source: "libssl-1_1-x64.dll"; DestDir: "{app}"
Source: "..\setting.ini"; DestDir: "{app}"
Source: "app.ico"; DestDir: "{app}"

[Icons]
;Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppName}.exe"
Name: "{userdesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; \
    IconFilename: "{app}\{#MyAppIcoName}"; Tasks: desktopicon

