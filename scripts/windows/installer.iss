; HT-76 Windows x64 installer. Apache-2.0; see LICENSE.
; Compiled by package-windows.py using a validated staging directory.
[Setup]
AppId=com.hikaritsai.ht76.installer
AppName=HT-76
AppVersion={#ProductVersion}
AppPublisher=Field Effect
DefaultDirName={autopf}\HT-76
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousSetupType=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
WizardStyle=modern
OutputDir={#OutputDirectory}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
SetupLogging=yes
UninstallDisplayName=HT-76 Plugins
InfoBeforeFile={#StageDirectory}\README.txt
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no

[Types]
Name: "full"; Description: "All plugin formats"
Name: "custom"; Description: "Choose plugin formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3"; Types: full
Name: "aax"; Description: "AAX Native (Pro Tools Developer; no PACE signature)"; Types: full

[Files]
Source: "{#StageDirectory}\VST3\HT-76.vst3\*"; DestDir: "{commoncf64}\VST3\HT-76.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDirectory}\AAX\HT-76.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\HT-76.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDirectory}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDirectory}\NOTICE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDirectory}\README.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDirectory}\build-info.json"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDirectory}\third_party\*"; DestDir: "{app}\third_party"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StageDirectory}\vc_redist.x64.exe"; Flags: dontcopy

; Inno Setup's uninstall log removes only files installed by this setup.
; Do not use wildcard UninstallDelete rules on shared plugin directories.
[Code]
var
  RuntimeNeedsRestart: Boolean;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  ExtractTemporaryFile('vc_redist.x64.exe');
  if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'),
    '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    Result := 'Could not start the Microsoft Visual C++ x64 runtime installer.';
    Exit;
  end;
  { 1638 means a newer shared runtime is already installed. }
  if (ResultCode <> 0) and (ResultCode <> 1638) and (ResultCode <> 3010) then
    Result := Format('Microsoft Visual C++ runtime installation failed (code %d).', [ResultCode]);
  if ResultCode = 3010 then
    RuntimeNeedsRestart := True;
end;

function NeedRestart: Boolean;
begin
  Result := RuntimeNeedsRestart;
end;
