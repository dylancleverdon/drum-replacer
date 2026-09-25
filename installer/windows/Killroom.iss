; Inno Setup script for the Windows installer (Killroom-Windows-Setup.exe).
; Installs Killroom.vst3 into C:\Program Files\Common Files\VST3, where Live looks for plugins
; and where the plugin's own Update button replaces it later.
;
;   ISCC.exe /DAppVersion=1.0.3 /DSourceDir=C:\path\to\Killroom.vst3 installer\windows\Killroom.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\build\Killroom_artefacts\Release\VST3\Killroom.vst3"
#endif

[Setup]
; Keep AppId the same forever so new versions install over old ones.
AppId={{6F3C2B1A-9D4E-4C8B-A7F2-5E1D0C9B8A73}
AppName=Killroom
AppVersion={#AppVersion}
AppVerName=Killroom {#AppVersion}
AppPublisher=dylancleverdon
AppPublisherURL=https://github.com/dylancleverdon/drum-replacer
AppUpdatesURL=https://github.com/dylancleverdon/drum-replacer/releases
VersionInfoVersion={#AppVersion}
; Holds only the uninstaller; the plugin itself goes into the VST3 folder below.
DefaultDirName={autopf}\Killroom
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; A running Live is handled in [Code] instead of asking to close it.
CloseApplications=no
OutputBaseFilename=Killroom-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Killroom

[Files]
Source: "{#SourceDir}\*"; DestDir: "{commoncf64}\VST3\Killroom.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[InstallDelete]
; Binaries moved aside by earlier updates (see CurStepChanged)
Type: files; Name: "{commoncf64}\VST3\Killroom.vst3\Contents\x86_64-win\Killroom.vst3.old-*"

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Killroom.vst3"

[Messages]
FinishedLabel=Killroom is installed. Restart Ableton Live and find it under Plug-Ins > VST3.

[Code]
// Live keeps the plugin binary locked while it's loaded, but Windows still allows renaming
// it. Move it aside so the install succeeds; the running Live keeps using the old copy.
procedure CurStepChanged(CurStep: TSetupStep);
var
  Binary: String;
begin
  if CurStep = ssInstall then
  begin
    Binary := ExpandConstant('{commoncf64}\VST3\Killroom.vst3\Contents\x86_64-win\Killroom.vst3');
    if FileExists(Binary) and not DeleteFile(Binary) then
      RenameFile(Binary, Binary + '.old-' + GetDateTimeString('yyyymmddhhnnss', #0, #0));
  end;
end;
