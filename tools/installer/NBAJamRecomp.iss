; NBAJamRecomp.iss - the Windows installer (roadmap R3.1), built with Inno Setup 6 in CI:
;
;   iscc /DAppVer=<version> /DSrc=<unpacked package folder> /O<out dir> NBAJamRecomp.iss
;
; <Src> is the Windows package without portable.txt, so the installed program keeps the
; user's settings, saves, mods and ROMs in %APPDATA%\NBAJamRecomp (src/app/paths.h), not in
; the install folder. Per-user: no administrator prompt. Uninstalling removes the program and
; leaves that data folder, unless the user says yes when asked (or passes /PURGE to a silent
; uninstall).

#ifndef AppVer
  #define AppVer "dev"
#endif
#ifndef Src
  #define Src "..\..\dist-installer\NBAJamRecomp"
#endif

[Setup]
AppId={{6B0E3C55-2B7A-4E0B-9C51-4E4A1D7F0A11}
AppName=NBA JAM Recomp
AppVersion={#AppVer}
AppPublisher=NBA JAM Recomp contributors
AppPublisherURL=https://github.com/richvam/NBA-JAMs-Recomp
DefaultDirName={autopf}\NBAJamRecomp
DefaultGroupName=NBA JAM Recomp
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=NBAJamRecomp-{#AppVer}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\NBAJamRecomp.exe
UninstallDisplayName=NBA JAM Recomp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Src}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion; Excludes: "portable.txt"

[Icons]
Name: "{autoprograms}\NBA JAM Recomp"; Filename: "{app}\NBAJamRecomp.exe"
Name: "{autoprograms}\NBA JAM Recomp ROMs folder"; Filename: "{userappdata}\NBAJamRecomp\roms"; Check: DataExists
Name: "{autodesktop}\NBA JAM Recomp"; Filename: "{app}\NBAJamRecomp.exe"; Tasks: desktopicon

[Dirs]
Name: "{userappdata}\NBAJamRecomp\roms"; Flags: uninsneveruninstall

[Run]
Filename: "{app}\NBAJamRecomp.exe"; Description: "{cm:LaunchProgram,NBA JAM Recomp}"; Flags: nowait postinstall skipifsilent

[Code]
function DataExists: Boolean;
begin
  Result := DirExists(ExpandConstant('{userappdata}\NBAJamRecomp\roms'));
end;

function PurgeAsked: Boolean;
var
  i: Integer;
begin
  Result := False;
  for i := 1 to ParamCount do
    if CompareText(ParamStr(i), '/PURGE') = 0 then Result := True;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  data: String;
  purge: Boolean;
begin
  if CurUninstallStep <> usPostUninstall then Exit;
  data := ExpandConstant('{userappdata}\NBAJamRecomp');
  if not DirExists(data) then Exit;
  purge := PurgeAsked;
  if (not purge) and (not UninstallSilent) then
    purge := MsgBox('Also delete your settings, saves, mods and ROMs in' + #13#10 + data + '?' + #13#10#13#10 +
                    'Choose No to keep them for a later install.', mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES;
  if purge then DelTree(data, True, True, True);
end;
