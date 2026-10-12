[Setup]
AppId={{04A15615-E2BD-4F06-B96C-91467826498B}}
AppName=Cummy
AppVersion=1.0.0
DefaultDirName={localappdata}\Programs\Cummy
DefaultGroupName=Cummy
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
OutputBaseFilename=cummy-setup-1.0.0
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes
UninstallDisplayName=Cummy
SetupIconFile=assets\cummY-app.ico
UninstallDisplayIcon={app}\cummY-app.ico

[Tasks]
Name: "addtopath"; Description: "Add Cummy to your user PATH"; GroupDescription: "Optional features:"; Flags: unchecked

[Files]
Source: "cummy.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "assets\cummY-app.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Cummy"; Filename: "{app}\cummy.exe"; Parameters: "help"; WorkingDir: "{app}"; IconFilename: "{app}\cummY-app.ico"

[Run]
Filename: "{app}\cummy.exe"; Parameters: "help"; Description: "Show Cummy help"; Flags: postinstall nowait skipifsilent unchecked

[Code]
function NormalizePathEntry(const Entry: String): String;
var
  Value: String;
begin
  Value := Lowercase(Trim(Entry));
  while (Length(Value) > 3) and (Value[Length(Value)] = '\') do
    Delete(Value, Length(Value), 1);
  Result := Value;
end;

function PathContains(const PathValue, Entry: String): Boolean;
var
  Remaining: String;
  Segment: String;
  Separator: Integer;
begin
  Remaining := PathValue;
  while Remaining <> '' do
  begin
    Separator := Pos(';', Remaining);
    if Separator = 0 then
    begin
      Segment := Trim(Remaining);
      Remaining := '';
    end
    else
    begin
      Segment := Trim(Copy(Remaining, 1, Separator - 1));
      Delete(Remaining, 1, Separator);
    end;
    if NormalizePathEntry(Segment) = NormalizePathEntry(Entry) then
    begin
      Result := True;
      Exit;
    end;
  end;
  Result := False;
end;

function RemovePathEntry(const PathValue, Entry: String): String;
var
  Segment: String;
  NewPath: String;
  StartAt: Integer;
  Separator: Integer;
  HasSegment: Boolean;
begin
  NewPath := '';
  StartAt := 1;
  HasSegment := False;
  while StartAt <= Length(PathValue) + 1 do
  begin
    Separator := Pos(';', Copy(PathValue, StartAt, Length(PathValue) - StartAt + 1));
    if Separator = 0 then
    begin
      Segment := Copy(PathValue, StartAt, Length(PathValue) - StartAt + 1);
      StartAt := Length(PathValue) + 2;
    end
    else
    begin
      Segment := Copy(PathValue, StartAt, Separator - 1);
      StartAt := StartAt + Separator;
    end;

    if NormalizePathEntry(Segment) <> NormalizePathEntry(Entry) then
    begin
      if HasSegment then NewPath := NewPath + ';';
      NewPath := NewPath + Segment;
      HasSegment := True;
    end;
  end;
  Result := NewPath;
end;

procedure AddToUserPath;
var
  UserPath: String;
  InstallPath: String;
begin
  InstallPath := ExpandConstant('{app}');
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UserPath) then
    UserPath := '';
  if PathContains(UserPath, InstallPath) then Exit;

  if UserPath = '' then
    UserPath := InstallPath
  else if UserPath[Length(UserPath)] = ';' then
    UserPath := UserPath + InstallPath
  else
    UserPath := UserPath + ';' + InstallPath;

  if not RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UserPath) then
    MsgBox('Cummy was installed, but Windows could not update your user PATH.', mbError, MB_OK);
end;

procedure RemoveFromUserPath;
var
  UserPath: String;
  InstallPath: String;
begin
  InstallPath := ExpandConstant('{app}');
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UserPath) then Exit;
  if not PathContains(UserPath, InstallPath) then Exit;

  UserPath := RemovePathEntry(UserPath, InstallPath);
  RegWriteExpandStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', UserPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
    AddToUserPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RemoveFromUserPath;
end;