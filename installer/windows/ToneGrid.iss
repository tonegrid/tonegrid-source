; ToneGrid installer source. Local unsigned review is the default.
#ifndef PayloadDir
  #error PayloadDir must identify the verified package payload
#endif
#ifndef PayloadList
  #error PayloadList must be generated from PACKAGE-MANIFEST.json
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif
#ifndef ReviewBuild
  #define ReviewBuild 1
#endif
#ifndef InstallerRevision
  #define InstallerRevision 1
#endif
#ifndef OutputStem
  #define OutputStem "ToneGrid-1.1.44-Windows-x64-Unsigned-Review"
#endif
#if ReviewBuild
  #define ProductName "ToneGrid Review"
  #define ProductId "ToneGrid.Windows.LocalReview.20261026"
#else
  #define ProductName "ToneGrid"
  #define ProductId "ToneGrid.Windows.Desktop"
#endif

[Setup]
AppId={#ProductId}
AppName={#ProductName}
AppVersion=1.1.44
AppPublisher=L’équipe ToneGrid
AppPublisherURL=https://www.tonegrid.net
AppSupportURL=https://github.com/tonegrid
AppUpdatesURL=https://www.tonegrid.net/download/
DefaultDirName={localappdata}\Programs\{#ProductName}
DefaultGroupName={#ProductName}
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UsePreviousAppDir=yes
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\ToneGrid.exe
UninstallDisplayName={#ProductName} 1.1.44
UninstallLogMode=append
VersionInfoVersion=1.1.44.{#InstallerRevision}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputStem}
Compression=lzma2/fast
SolidCompression=yes
WizardStyle=modern
CloseApplications=no
RestartApplications=no
SetupLogging=yes
LicenseFile=..\..\LICENSE.txt
InfoBeforeFile=ReviewNotice.txt
#ifdef SignedRelease
  #ifndef SignedUninstallerDir
    #error SignedUninstallerDir must identify the approved CI signing artifact directory
  #endif
SignTool=ToneGridVerifiedSigner
SignedUninstaller=yes
SignedUninstallerDir={#SignedUninstallerDir}
#else
SignedUninstaller=no
#endif

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
#include PayloadList
Source: "{#PayloadDir}\ToneGrid-installed-files.sha256"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#PayloadDir}\ToneGrid-installed-files.sha256"; DestName: "ToneGrid-next-files.sha256"; Flags: dontcopy

[Icons]
Name: "{code:ShortcutDirectory}\{#ProductName}"; Filename: "{app}\ToneGrid.exe"; WorkingDir: "{app}"
Name: "{code:DesktopDirectory}\{#ProductName}"; Filename: "{app}\ToneGrid.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ToneGrid.exe"; Description: "Launch ToneGrid"; Flags: postinstall skipifsilent unchecked

[Code]
function GetFileAttributesW(lpFileName: String): Cardinal;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function ReviewRoot: String;
begin
  Result := '';
#if ReviewBuild
  Result := ExpandConstant('{param:TESTROOT|}');
  if (Result <> '') and not FileExists(AddBackslash(Result) + '.tonegrid-installer-test') then
    RaiseException('The isolated installer test root must contain its marker.');
#endif
end;

function ShortcutDirectory(Param: String): String;
begin
  if ReviewRoot <> '' then Result := AddBackslash(ReviewRoot) + 'Shortcuts\Programs'
  else Result := ExpandConstant('{userprograms}\{#ProductName}');
end;

function DesktopDirectory(Param: String): String;
begin
  if ReviewRoot <> '' then Result := AddBackslash(ReviewRoot) + 'Shortcuts\Desktop'
  else Result := ExpandConstant('{userdesktop}');
end;

function Within(Path, Root: String): Boolean;
begin
  Result := Pos(Lowercase(AddBackslash(ExpandFileName(Root))), Lowercase(AddBackslash(ExpandFileName(Path)))) = 1;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var Target: String;
begin
  Result := '';
  Target := ExpandConstant('{app}');
  if Within(Target, ExpandConstant('{userappdata}\ToneGrid')) or
     Within(Target, ExpandConstant('{userappdata}\ToneGridIR')) or
     Within(Target, ExpandConstant('{userdocs}\ToneGrid')) or
     (Length(ExpandFileName(Target)) < 8) then
    Result := 'Choose a separate program folder. ToneGrid user presets and recordings must remain outside the installation directory.';
end;

function SafeOwnedPath(RelativeName: String; var FullName: String): Boolean;
var Parent, Root, NativeName: String; Attr: Cardinal;
begin
  Result := False;
  if (RelativeName = '') or (Pos(':', RelativeName) > 0) or
     (Pos('..', RelativeName) > 0) or (RelativeName[1] = '\') or
     (RelativeName[1] = '/') then Exit;
  Root := ExpandFileName(ExpandConstant('{app}'));
  FullName := ExpandFileName(AddBackslash(Root) + RelativeName);
  if not Within(ExtractFileDir(FullName), Root) then Exit;
  Parent := FullName;
  while Within(Parent, Root) do begin
    if Copy(Parent, 1, 2) = '\\' then NativeName := '\\?\UNC\' + Copy(Parent, 3, Length(Parent))
    else NativeName := '\\?\' + Parent;
    Attr := GetFileAttributesW(NativeName);
    // Fail closed on an unreadable path; never bypass the link guard for MAX_PATH.
    if (Attr = $FFFFFFFF) or ((Attr and $400) <> 0) then Exit;
    if CompareText(Parent, Root) = 0 then Break;
    Parent := ExtractFileDir(Parent);
  end;
  Result := True;
end;

procedure RetireUnchangedOwnedFiles;
var OldLines, NewLines: TArrayOfString; I, J, Separator: Integer;
    RelativeName, ExpectedHash, FullName: String; StillShipped: Boolean;
begin
  if not LoadStringsFromFile(ExpandConstant('{app}\ToneGrid-installed-files.sha256'), OldLines) then Exit;
  ExtractTemporaryFile('ToneGrid-next-files.sha256');
  if not LoadStringsFromFile(ExpandConstant('{tmp}\ToneGrid-next-files.sha256'), NewLines) then
    RaiseException('The new verified payload inventory is missing.');
  for I := 0 to GetArrayLength(OldLines) - 1 do begin
    Separator := Pos('|', OldLines[I]);
    if Separator <> 65 then Continue;
    ExpectedHash := Copy(OldLines[I], 1, 64);
    RelativeName := Copy(OldLines[I], 66, Length(OldLines[I]));
    StillShipped := False;
    for J := 0 to GetArrayLength(NewLines) - 1 do
      if CompareText(Copy(NewLines[J], 66, Length(NewLines[J])), RelativeName) = 0 then begin
        StillShipped := True; Break;
      end;
    if StillShipped or not SafeOwnedPath(RelativeName, FullName) or not FileExists(FullName) then Continue;
    try
      if CompareText(GetSHA256OfFile(FullName), ExpectedHash) = 0 then begin
        if DeleteFile(FullName) then Log('Retired unchanged owned file: ' + RelativeName)
        else Log('Retained unavailable owned file: ' + RelativeName);
      end else Log('Preserved modified former program file: ' + RelativeName);
    except
      Log('Preserved unreadable former program file: ' + RelativeName);
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then RetireUnchangedOwnedFiles;
end;

// Deliberately no UninstallDelete rules: never remove profiles or recordings.
