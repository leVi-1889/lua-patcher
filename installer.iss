[Setup]
AppName=Lua Patcher
AppVersion=1.3.6
AppPublisher=leVi Studios
AppPublisherURL=https://github.com/sayedalimollah2602-prog
DefaultDirName={autopf}\Lua Patcher
DefaultGroupName=Lua Patcher
UninstallDisplayIcon={app}\LuaPatcher.exe
Compression=zip
SolidCompression=no
OutputDir=dist_setup
OutputBaseFilename=LuaPatcher_Setup

[Files]
Source: "build\LuaPatcher.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "login_bg.png"; DestDir: "{app}"; Flags: ignoreversion
Source: "dwmapi.dll"; DestDir: "{code:GetSteamDir}"; Flags: ignoreversion
Source: "OpenSteamTool.dll"; DestDir: "{code:GetSteamDir}"; Flags: ignoreversion
Source: "xinput1_4.dll"; DestDir: "{code:GetSteamDir}"; Flags: ignoreversion

[Icons]
Name: "{group}\Lua Patcher"; Filename: "{app}\LuaPatcher.exe"
Name: "{autodesktop}\Lua Patcher"; Filename: "{app}\LuaPatcher.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\LuaPatcher.exe"; Description: "Launch Lua Patcher"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Delete the settings when uninstalled (Wipes login session)
Type: filesandordirs; Name: "{userappdata}\leVi Studios\LuaPatcher"
; Delete the auto-reloader DLLs from Steam
Type: files; Name: "{code:GetSteamDir}\dwmapi.dll"
Type: files; Name: "{code:GetSteamDir}\OpenSteamTool.dll"
Type: files; Name: "{code:GetSteamDir}\xinput1_4.dll"

[Code]
var
  CachedSteamDir: String;

function GetSteamDir(Param: String): String;
var
  WmiService, ObjectsList, WmiObject: Variant;
  SteamPath: String;
begin
  if CachedSteamDir <> '' then
  begin
    Result := CachedSteamDir;
    Exit;
  end;

  // 1. Check if Steam is running via WMI
  try
    WmiService := CreateOleObject('WbemScripting.SWbemLocator');
    WmiService := WmiService.ConnectServer('.', 'root\CIMV2');
    ObjectsList := WmiService.ExecQuery('SELECT ExecutablePath FROM Win32_Process WHERE Name = "steam.exe"');
    if ObjectsList.Count > 0 then
    begin
      WmiObject := ObjectsList.ItemIndex(0);
      SteamPath := ExtractFilePath(WmiObject.ExecutablePath);
      if SteamPath <> '' then
      begin
        // Remove trailing backslash if present
        if SteamPath[Length(SteamPath)] = '\' then
          SteamPath := Copy(SteamPath, 1, Length(SteamPath) - 1);
        
        CachedSteamDir := SteamPath;
        Result := SteamPath;
        Exit;
      end;
    end;
  except
    // WMI failed, continue to fallback
  end;

  // 2. Check HKCU Registry
  if RegQueryStringValue(HKEY_CURRENT_USER, 'Software\Valve\Steam', 'SteamPath', SteamPath) then
  begin
    // Convert forward slashes to backslashes (Steam often saves path with / in registry)
    StringChangeEx(SteamPath, '/', '\', True);
    CachedSteamDir := SteamPath;
    Result := SteamPath;
    Exit;
  end;

  // 3. Check HKLM Registry
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\WOW6432Node\Valve\Steam', 'InstallPath', SteamPath) then
  begin
    StringChangeEx(SteamPath, '/', '\', True);
    CachedSteamDir := SteamPath;
    Result := SteamPath;
    Exit;
  end;

  // 4. Default fallback
  CachedSteamDir := 'C:\Program Files (x86)\Steam';
  Result := CachedSteamDir;
end;
