; TX2000W Windows installer. Built by package-release.ps1, which passes the
; defines below.
#ifndef AppVersion
  #error AppVersion is required. Run package-release.ps1.
#endif
#ifndef Architecture
  #error Architecture is required. Run package-release.ps1.
#endif
#ifndef AllowedArchitectures
  #error AllowedArchitectures is required. Run package-release.ps1.
#endif
#ifndef Install64Architectures
  #error Install64Architectures is required. Run package-release.ps1.
#endif
#ifndef OutputBaseFilename
  #error OutputBaseFilename is required. Run package-release.ps1.
#endif
#ifndef OutputDir
  #error OutputDir is required. Run package-release.ps1.
#endif
#ifndef BuildRoot
  #error BuildRoot is required. Run package-release.ps1.
#endif
#ifndef RepoRoot
  #error RepoRoot is required. Run package-release.ps1.
#endif

[Setup]
AppId=tokyo.studio-r.tx2000w
AppName=TX2000W
AppVersion={#AppVersion}
AppVerName=TX2000W {#AppVersion} ({#Architecture})
AppPublisher=STUDIO-R
AppPublisherURL=https://studio-r.tokyo
DefaultDirName={autopf}\STUDIO-R\TX2000W
DefaultGroupName=TX2000W
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
ArchitecturesAllowed={#AllowedArchitectures}
ArchitecturesInstallIn64BitMode={#Install64Architectures}
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
LicenseFile={#RepoRoot}\LICENSE
VersionInfoVersion={#AppVersion}
VersionInfoCompany=STUDIO-R
VersionInfoDescription=TX2000W Windows {#Architecture} Installer
VersionInfoProductName=TX2000W
VersionInfoProductVersion={#AppVersion}

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "standalone"; Description: "TX2000W standalone application"; Types: full
Name: "vst3"; Description: "TX2000W VST3 plug-in"; Types: full

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Components: standalone; Flags: unchecked

[Files]
Source: "{#BuildRoot}\Standalone Plugin\TX2000W.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#RepoRoot}\NOTICE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#BuildRoot}\VST3\TX2000W.vst3\*"; DestDir: "{commoncf64}\VST3\TX2000W.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\TX2000W.vst3"; Components: vst3

[Icons]
Name: "{group}\TX2000W"; Filename: "{app}\TX2000W.exe"; Components: standalone
Name: "{autodesktop}\TX2000W"; Filename: "{app}\TX2000W.exe"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\TX2000W.exe"; Description: "Launch TX2000W"; Components: standalone; Flags: nowait postinstall skipifsilent
