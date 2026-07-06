; QT-AGV Windows 安装包脚本 (Inno Setup)
; 由 build_installer.bat 自动调用，一般无需手动运行。
; 手动编译: ISCC.exe /DMyAppVersion=1.0.0 installer.iss

#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#define MyAppName "QT-AGV 移动机器人管理平台"
#define MyAppExeName "QT-AGV.exe"
#define MyAppPublisher "QT-AGV"

[Setup]
AppId={{CBBC6B03-6BF6-40FB-90F5-8BF31C48A9C2}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\QT-AGV
DefaultGroupName=QT-AGV
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=QT-AGV-Setup-{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\app-icon.ico
SetupIconFile=..\src\resources\app-icon.ico

[Languages]
; Inno Setup 官方安装包默认不附带简体中文翻译文件，这里用 build_installer.bat
; 自动下载好的 Languages\ChineseSimplified.isl（随仓库一起提交，离线也能编译）。
Name: "chinesesimp"; MessagesFile: "Languages\ChineseSimplified.isl"

[Files]
; dist\ 由 build_installer.bat 用 windeployqt 生成，包含 exe + 所有依赖 DLL/插件
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; 图标文件：安装到 {app}，供桌面/开始菜单快捷方式引用
Source: "..\src\resources\app-icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"

[Icons]
Name: "{group}\QT-AGV"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app-icon.ico"
Name: "{group}\卸载 QT-AGV"; Filename: "{uninstallexe}"
Name: "{autodesktop}\QT-AGV"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app-icon.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "立即运行 QT-AGV"; Flags: nowait postinstall skipifsilent
