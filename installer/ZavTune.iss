; Inno Setup script: installs ZavTune.vst3 into the standard VST3 folder FL Studio scans.
#define AppVersion "1.0.0"

[Setup]
AppName=ZavTune
AppVersion={#AppVersion}
AppPublisher=ZaviiTX
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
UninstallFilesDir={commoncf64}\VST3\ZavTune.vst3
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\dist
OutputBaseFilename=ZavTune Installer
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\build\ZavTune_artefacts\Release\VST3\ZavTune.vst3\*"; DestDir: "{app}\ZavTune.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=ZavTune is installed.%n%nIn FL Studio: Options > Manage plugins > Find installed plugins.
