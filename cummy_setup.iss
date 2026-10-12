[Setup]
AppId={{04A15615-E2BD-4F06-B96C-91467826498B}}
AppName=Cummy
AppVersion=1.0.2
DefaultDirName={localappdata}\Programs\Cummy
DefaultGroupName=Cummy
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=cummy-setup-1.0.2
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
Source: "output\cummy.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "assets\cummY-app.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\Cummy"; Filename: "{sys}\cmd.exe"; Parameters: "/k """"{app}\cummy.exe"" help"""; WorkingDir: "{app}"; IconFilename: "{app}\cummY-app.ico"
Name: "{autoprograms}\Cummy\GitHub repository"; Filename: "https://github.com/CookieCums/cummy"
Name: "{autoprograms}\Cummy\Download latest release"; Filename: "https://github.com/CookieCums/cummy/releases/latest"

[Run]
Filename: "{sys}\cmd.exe"; Parameters: "/k """"{app}\cummy.exe"" help"""; Description: "Show Cummy help"; Flags: postinstall nowait skipifsilent unchecked

[Code]
const
  CummyUninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{04A15615-E2BD-4F06-B96C-91467826498B}}_is1';

var
  MaintenancePage: TInputOptionWizardPage;
  PreviousUninstallString: String;
  PreviousUninstallFilename: String;
  PreviousUninstallParameters: String;
  HasPreviousInstallation: Boolean;
  ExitAfterUninstall: Boolean;

procedure ParseUninstallCommandLine(const CommandLine: String;
  var Filename, Parameters: String);
var
  ClosingQuote: Integer;
  ExeExtension: Integer;
begin
  Filename := '';
  Parameters := '';
  if (CommandLine = '') then Exit;

  if CommandLine[1] = '"' then
  begin
    ClosingQuote := Pos('"', Copy(CommandLine, 2, Length(CommandLine) - 1));
    if ClosingQuote = 0 then Exit;
    Filename := Copy(CommandLine, 2, ClosingQuote - 1);
    Parameters := Trim(Copy(CommandLine, ClosingQuote + 2, Length(CommandLine)));
  end
  else
  begin
    ExeExtension := Pos('.exe', Lowercase(CommandLine));
    if ExeExtension = 0 then
      Filename := Trim(CommandLine)
    else
    begin
      Filename := Trim(Copy(CommandLine, 1, ExeExtension + 3));
      Parameters := Trim(Copy(CommandLine, ExeExtension + 4, Length(CommandLine)));
    end;
  end;
end;

function InitializeSetup: Boolean;
begin
  HasPreviousInstallation :=
    RegQueryStringValue(HKEY_CURRENT_USER, CummyUninstallKey, 'UninstallString', PreviousUninstallString);
  if HasPreviousInstallation then
  begin
    ParseUninstallCommandLine(PreviousUninstallString,
      PreviousUninstallFilename, PreviousUninstallParameters);
    if PreviousUninstallFilename = '' then
    begin
      MsgBox('The existing Cummy uninstaller command is invalid. Setup cannot continue.',
        mbError, MB_OK);
      Result := False;
      Exit;
    end;
  end;
  Result := True;
end;

procedure InitializeWizard;
begin
  MaintenancePage := CreateInputOptionPage(wpWelcome,
    'Cummy is already installed',
    'Choose what you want to do with the existing installation.',
    'Repair and reinstall keep the current location. Change location removes the old copy after you confirm the new installation.',
    True, False);
  MaintenancePage.Add('Repair / reinstall Cummy');
  MaintenancePage.Add('Change the installation location');
  MaintenancePage.Add('Uninstall Cummy');
  MaintenancePage.SelectedValueIndex := 0;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := False;
  if (PageID = MaintenancePage.ID) and not HasPreviousInstallation then
    Result := True
  else if (PageID = wpSelectDir) and HasPreviousInstallation and
    (MaintenancePage.SelectedValueIndex <> 1) then
    Result := True;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;
  if (CurPageID = MaintenancePage.ID) and (MaintenancePage.SelectedValueIndex = 2) then
  begin
    if MsgBox('Are you sure you want to uninstall Cummy?', mbConfirmation, MB_YESNO) <> IDYES then
    begin
      Result := False;
      Exit;
    end;

    if not Exec(PreviousUninstallFilename, PreviousUninstallParameters, '',
      SW_SHOWNORMAL, ewWaitUntilTerminated, ResultCode) then
    begin
      MsgBox('The Cummy uninstaller could not be started: ' + SysErrorMessage(ResultCode), mbError, MB_OK);
      Result := False;
      Exit;
    end;

    if ResultCode <> 0 then
    begin
      MsgBox('The Cummy uninstaller returned exit code ' + IntToStr(ResultCode) + '.', mbError, MB_OK);
      Result := False;
      Exit;
    end;

    if RegKeyExists(HKEY_CURRENT_USER, CummyUninstallKey) then
    begin
      MsgBox('Cummy is still installed. The installer will not continue.', mbInformation, MB_OK);
      Result := False;
      Exit;
    end;

    ExitAfterUninstall := True;
    WizardForm.Close;
    Result := False;
  end;
end;

procedure CancelButtonClick(CurPageID: Integer; var Cancel, Confirm: Boolean);
begin
  if ExitAfterUninstall then
  begin
    Cancel := True;
    Confirm := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  Result := '';
  if HasPreviousInstallation and (MaintenancePage.SelectedValueIndex = 1) then
  begin
    if not Exec(PreviousUninstallFilename,
      Trim(PreviousUninstallParameters + ' /VERYSILENT /SUPPRESSMSGBOXES /NORESTART'),
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    begin
      Result := 'The previous Cummy installation could not be removed: ' + SysErrorMessage(ResultCode);
      Exit;
    end;

    if ResultCode <> 0 then
    begin
      Result := 'The previous Cummy uninstaller returned exit code ' + IntToStr(ResultCode) + '.';
      Exit;
    end;

    if RegKeyExists(HKEY_CURRENT_USER, CummyUninstallKey) then
      Result := 'The previous Cummy installation is still present. Setup cannot continue.';
  end;
end;

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