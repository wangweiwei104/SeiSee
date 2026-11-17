# Update these defaults if your Qt, MinGW, or Inno Setup installation uses
# different directories. The package version is read from SeiSeeMp\mainwindow.h.
param(
    [string]$QtBin = "C:\Qt\5.15.2\mingw81_64\bin",
    [string]$MinGWBin = "C:\Qt\Tools\mingw810_64\bin",
    [string]$InnoSetupCompiler = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    [int]$Jobs = [Environment]::ProcessorCount
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$versionHeader = Join-Path $repoRoot "SeiSeeMp\mainwindow.h"
if (-not (Test-Path -LiteralPath $versionHeader -PathType Leaf)) {
    throw "Version header not found: $versionHeader"
}

$headerContent = Get-Content -LiteralPath $versionHeader -Raw
$versionDefinitions = [regex]::Matches(
    $headerContent,
    '(?m)^[ \t]*#[ \t]*define[ \t]+VERSION[ \t]+"([^"]+)"[ \t\r]*$'
)
if ($versionDefinitions.Count -ne 1) {
    throw "Expected exactly one #define VERSION `"value`" in $versionHeader; found $($versionDefinitions.Count)."
}
$Version = $versionDefinitions[0].Groups[1].Value

$qmake = Join-Path $QtBin "qmake.exe"
$windeployqt = Join-Path $QtBin "windeployqt.exe"
$make = Join-Path $MinGWBin "mingw32-make.exe"
foreach ($tool in @($qmake, $windeployqt, $make, $InnoSetupCompiler)) {
    if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) {
        throw "Required packaging tool not found: $tool"
    }
}
if ($Version -notmatch '^[A-Za-z0-9][A-Za-z0-9 ._-]*$') {
    throw 'Version may contain only letters, numbers, spaces, dots, underscores, and hyphens.'
}
$numericVersionMatch = [regex]::Match(
    $Version,
    '^(?<numericVersion>\d+(?:\.\d+){0,3})(?:[-+].*)?$'
)
if (-not $numericVersionMatch.Success) {
    throw "Version '$Version' cannot be represented as a Windows file version."
}

$versionParts = @($numericVersionMatch.Groups["numericVersion"].Value.Split('.'))
foreach ($part in $versionParts) {
    $component = 0
    if (-not [int]::TryParse($part, [ref]$component) -or $component -gt 65535) {
        throw "Version '$Version' contains a file-version component outside the supported range."
    }
}
while ($versionParts.Count -lt 4) {
    $versionParts += "0"
}
$versionInfoVersion = $versionParts -join "."

$safeVersion = $Version -replace '[^A-Za-z0-9._-]', '-'
$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ("SeiSee-package-" + [guid]::NewGuid())
$stageDir = Join-Path $tempRoot "app"
$outputDir = Join-Path $repoRoot "dist\windows"
$outputName = "SeiSee-$safeVersion-Setup"
$issPath = Join-Path $tempRoot "SeiSee.iss"
$buildDir = Join-Path $repoRoot "build\windows-release"
$exePath = Join-Path $buildDir "SeiSeeMp\release\SeiSeeMp.exe"
$iconPath = Join-Path $repoRoot "SeiSeeMp\images\SeiSeeSetup.ico"

New-Item -ItemType Directory -Path $stageDir, $outputDir, $buildDir -Force | Out-Null
try {
    $env:PATH = "$QtBin;$MinGWBin;$env:PATH"
    Push-Location $buildDir
    try {
        & $qmake (Join-Path $repoRoot "GxApps.pro") "-r" "CONFIG+=release" "CONFIG-=debug"
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed with exit code $LASTEXITCODE"
        }
        & $make "-j$Jobs"
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
        throw "Expected release executable not found: $exePath"
    }
    if (-not (Test-Path -LiteralPath $iconPath -PathType Leaf)) {
        throw "Installer icon not found: $iconPath"
    }

    Copy-Item -LiteralPath $exePath -Destination $stageDir
    Push-Location $stageDir
    try {
        & $windeployqt "SeiSeeMp.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "windeployqt failed with exit code $LASTEXITCODE"
        }
    } finally {
        Pop-Location
    }
    Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $stageDir

    $stageImagesDir = Join-Path $stageDir "images"
    New-Item -ItemType Directory -Path $stageImagesDir -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repoRoot "SeiSeeMp\images\segyfile.ico") -Destination $stageImagesDir

    $iss = @"
[Setup]
AppId={{4F15CB68-BEB3-4C36-9E98-61F9BCAC8125}
AppName=SeiSee
AppVersion=$Version
VersionInfoDescription=SeiSee Setup
VersionInfoVersion=$versionInfoVersion
VersionInfoProductName=SeiSee
VersionInfoProductVersion=$versionInfoVersion
VersionInfoCopyright=Copyright (C) WW. All rights reserved.
DefaultDirName={autopf}\SeiSee
DisableDirPage=no
DefaultGroupName=SeiSee
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=$outputDir
OutputBaseFilename=$outputName
SetupIconFile=$iconPath
UninstallDisplayIcon={app}\SeiSeeMp.exe
ChangesAssociations=yes
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"
Name: "associatefiles"; Description: "Associate .sgy and .segy files with SeiSee"; GroupDescription: "File associations:"

[Files]
Source: "$stageDir\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
Root: HKA; Subkey: "Software\Classes\.sgy"; ValueType: string; ValueName: ""; ValueData: "SeiSee.SegyFile"; Flags: uninsdeletevalue; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\.sgy\OpenWithProgids"; ValueType: string; ValueName: "SeiSee.SegyFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\.segy"; ValueType: string; ValueName: ""; ValueData: "SeiSee.SegyFile"; Flags: uninsdeletevalue; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\.segy\OpenWithProgids"; ValueType: string; ValueName: "SeiSee.SegyFile"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\SeiSee.SegyFile"; ValueType: string; ValueName: ""; ValueData: "SEG-Y Seismic Data File"; Flags: uninsdeletekey; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\SeiSee.SegyFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\images\segyfile.ico,0"; Flags: uninsdeletekey; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\SeiSee.SegyFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\SeiSeeMp.exe"" ""%1"""; Flags: uninsdeletekey; Tasks: associatefiles
Root: HKA; Subkey: "Software\Classes\Applications\SeiSeeMp.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "SeiSee"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\SeiSeeMp.exe\SupportedTypes"; ValueType: string; ValueName: ".sgy"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\SeiSeeMp.exe\SupportedTypes"; ValueType: string; ValueName: ".segy"; ValueData: ""; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\SeiSeeMp.exe\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\images\segyfile.ico,0"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Applications\SeiSeeMp.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\SeiSeeMp.exe"" ""%1"""; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\wangweiwei104\SeiSee\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "SeiSee Seismic Data Viewer"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\wangweiwei104\SeiSee\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "SeiSee"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\wangweiwei104\SeiSee\Capabilities\FileAssociations"; ValueType: string; ValueName: ".sgy"; ValueData: "SeiSee.SegyFile"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\wangweiwei104\SeiSee\Capabilities\FileAssociations"; ValueType: string; ValueName: ".segy"; ValueData: "SeiSee.SegyFile"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "SeiSee"; ValueData: "Software\wangweiwei104\SeiSee\Capabilities"; Flags: uninsdeletevalue

[Icons]
Name: "{autoprograms}\SeiSee"; Filename: "{app}\SeiSeeMp.exe"; AppUserModelID: "wangweiwei104.SeiSee.SeiSeeMp"
Name: "{autodesktop}\SeiSee"; Filename: "{app}\SeiSeeMp.exe"; Tasks: desktopicon; AppUserModelID: "wangweiwei104.SeiSee.SeiSeeMp"

[Run]
Filename: "{app}\SeiSeeMp.exe"; Description: "Launch SeiSee"; Flags: postinstall nowait skipifsilent
"@
    Set-Content -LiteralPath $issPath -Value $iss -Encoding Ascii

    & $InnoSetupCompiler "/O$outputDir" "/F$outputName" $issPath
    if ($LASTEXITCODE -ne 0) {
        throw "Inno Setup compilation failed with exit code $LASTEXITCODE"
    }

    $installerPath = Join-Path $outputDir "$outputName.exe"
    & (Join-Path $PSScriptRoot "set-windows-version-language.ps1") `
        -ExecutablePath $installerPath

    Write-Output "Created installer: $installerPath"
} finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
}
