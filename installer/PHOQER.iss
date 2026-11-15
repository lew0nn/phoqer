; PHOQER installer: dark wizard with the pixel seal over a city at sunset (picked 11 Nov 2026).
; Built by build-release.ps1, which passes the version: ISCC /DMyAppVersion=1.0.1 installer\PHOQER.iss
#ifndef MyAppVersion
  #define MyAppVersion "1.0.1"
#endif
#define MyAppName "PHOQER"
#define MyAppPublisher "LWNX DSP"
#define MyAppExeName "PHOQER.exe"

[Setup]
AppId={{7506D888-8BA0-473C-B7E2-5D9A1463D98B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/lew0nn/phoqer
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=(C) 2026 lewonn / LWNX DSP
VersionInfoDescription={#MyAppName} Setup
DefaultDirName={autopf}\LWNX DSP\PHOQER
DefaultGroupName=LWNX DSP\PHOQER
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\dist\release
OutputBaseFilename=PHOQER-{#MyAppVersion}-Windows-x64-Setup
SetupIconFile=..\resources\branding\app\phoqer-app-icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
; The look: dark wizard, the pixel seal over a city at sunset (art at whole pixels, so size 100%).
WizardSizePercent=100
WizardStyle=modern dark
WizardBackColor=#080208
DisableWelcomePage=no
WizardImageFile=assets\wizard.png,assets\wizard-200.png
WizardSmallImageFile=assets\wizard-small.png,assets\wizard-small-200.png
WizardImageStretch=no
WizardImageBackColor=#080208
WizardSmallImageBackColor=#080208

[LangOptions]
DialogFontName=Segoe UI
DialogFontSize=9
WelcomeFontName=Segoe UI
WelcomeFontSize=12

[Messages]
WelcomeLabel2=This will install [name/ver] on your computer: the VST3 plug-in for your DAW and the standalone app.%n%nInside: seals that sing.

[Types]
Name: "full"; Description: "Full installation (VST3 and standalone app)"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 plug-in (Common Files\VST3)"; Types: full custom
Name: "standalone"; Description: "Standalone app"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Components: standalone

[Files]
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\NOTICE.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
; An update started by PHOQER passes /VST3DIR="..." (the folder its PHOQER.vst3 was loaded from), so a plug-in kept
; outside Common Files\VST3 is replaced where it is instead of getting a second copy.
Source: "..\dist\PHOQER.vst3\*"; DestDir: "{param:VST3DIR|{commoncf64}\VST3}\PHOQER.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\Standalone\*"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PHOQER"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Components: standalone
Name: "{autodesktop}\PHOQER"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Components: standalone; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Wake up the seals (launch PHOQER)"; Components: standalone; Flags: nowait postinstall skipifsilent

[Code]
// Focus goes to Next on every page, so no list or box shows the bright selection colour.
procedure CurPageChanged(CurPageID: Integer);
begin
  if WizardForm.NextButton.Visible and WizardForm.NextButton.Enabled then
    WizardForm.ActiveControl := WizardForm.NextButton;     // not on Installing, where Next is hidden
end;
