; DEATH DEALER DRUMS Windows Installer
; INFERNO TONES
; Inno Setup 6

#define AppName        "DEATH DEALER DRUMS"
#ifndef AppVersion
  #define AppVersion   "1.0.0"
#endif
#define AppPublisher   "INFERNO TONES"
#define AppURL         "https://myinferno.online/"
#define VST3Src        "..\..\DELIVERABLES\Windows\VST3\DEATH DEALER DRUMS.vst3"
#define StandaloneSrc  "..\..\DELIVERABLES\Windows\Standalone\DEATH DEALER DRUMS.exe"

[Setup]
AppId={{A86D4A6C-8A59-4B2B-9C57-DAA37E220C5B}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL={#AppURL}
DefaultDirName={autopf}\INFERNO TONES\DEATH DEALER DRUMS
DefaultGroupName={#AppPublisher}
OutputDir=.
OutputBaseFilename=DEATH_DEALER_DRUMS_Installer_v{#AppVersion}_Windows
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayName={#AppName}
UninstallDisplayIcon={code:GetStandaloneDir}\DEATH DEALER DRUMS.exe
DisableDirPage=yes
MinVersion=10.0

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "vst3";       Description: "VST3 Plugin (for DAWs)";                    Types: full compact custom; Flags: fixed
Name: "standalone"; Description: "Standalone Application (run without a DAW)"; Types: full custom

[Dirs]
Name: "{code:GetVST3Dir}\DEATH DEALER DRUMS.vst3";  Components: vst3
Name: "{code:GetStandaloneDir}";                     Components: standalone

[Files]
Source: "{#VST3Src}\*"; DestDir: "{code:GetVST3Dir}\DEATH DEALER DRUMS.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
Source: "{#StandaloneSrc}"; DestDir: "{code:GetStandaloneDir}"; Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\DEATH DEALER DRUMS (Standalone)"; Filename: "{code:GetStandaloneDir}\DEATH DEALER DRUMS.exe"; Components: standalone
Name: "{group}\Uninstall DEATH DEALER DRUMS"; Filename: "{uninstallexe}"
Name: "{autodesktop}\DEATH DEALER DRUMS"; Filename: "{code:GetStandaloneDir}\DEATH DEALER DRUMS.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut for the Standalone app"; Components: standalone

[Run]
Filename: "{code:GetStandaloneDir}\DEATH DEALER DRUMS.exe"; Description: "Launch DEATH DEALER DRUMS now"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{code:GetVST3Dir}\DEATH DEALER DRUMS.vst3"
Type: files; Name: "{code:GetStandaloneDir}\DEATH DEALER DRUMS.exe"

[Code]
var
  VST3DirPage: TInputDirWizardPage;
  StandaloneDirPage: TInputDirWizardPage;

function GetVST3Dir(Param: String): String;
begin
  Result := VST3DirPage.Values[0];
end;

function GetStandaloneDir(Param: String): String;
begin
  Result := StandaloneDirPage.Values[0];
end;

procedure InitializeWizard;
begin
  VST3DirPage := CreateInputDirPage(
    wpSelectComponents,
    'VST3 Plugin Install Location',
    'Where should the DEATH DEALER DRUMS VST3 plugin be installed?',
    'The folder below is the standard Windows VST3 location. Most DAWs scan here automatically. You can change it if your DAW uses a custom VST3 folder.',
    False, '');
  VST3DirPage.Add('VST3 folder:');
  VST3DirPage.Values[0] := ExpandConstant('{commoncf}\VST3');

  StandaloneDirPage := CreateInputDirPage(
    VST3DirPage.ID,
    'Standalone App Install Location',
    'Where should the DEATH DEALER DRUMS Standalone app be installed?',
    'Choose the folder where DEATH DEALER DRUMS.exe will be placed.',
    False, '');
  StandaloneDirPage.Add('Standalone folder:');
  StandaloneDirPage.Values[0] := ExpandConstant('{autopf}\INFERNO TONES\DEATH DEALER DRUMS');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = StandaloneDirPage.ID) and (not WizardIsComponentSelected('standalone')) then
    Result := True;
  if (PageID = VST3DirPage.ID) and (not WizardIsComponentSelected('vst3')) then
    Result := True;
end;

function InitializeUninstall(): Boolean;
begin
  Result := MsgBox(
    'This will remove DEATH DEALER DRUMS from your computer.'#13#10#13#10
    + 'Your user presets and DAW projects will NOT be deleted.'#13#10#13#10
    + 'Continue?',
    mbConfirmation, MB_YESNO) = IDYES;
end;
