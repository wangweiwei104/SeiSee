param(
    [string]$QtBin = $(if ($env:QTDIR) { Join-Path $env:QTDIR "bin" } elseif ($env:OS -eq "Windows_NT") { "C:\Qt\5.15.2\mingw81_64\bin" } else { "" }),
    [string]$MinGWBin = "C:\Qt\Tools\mingw810_64\bin",
    [int]$Size = 512
)

$ErrorActionPreference = "Stop"

if ($Size -le 0) {
    throw "Size must be a positive integer."
}

$isWindows = $env:OS -eq "Windows_NT"
$qmakeName = if ($isWindows) { "qmake.exe" } else { "qmake" }
$qmake = if ($QtBin) { Join-Path $QtBin $qmakeName } else { $qmakeName }
if ($QtBin -and -not (Test-Path -LiteralPath $qmake -PathType Leaf)) {
    throw "qmake was not found at '$qmake'. Pass the Qt bin directory with -QtBin."
}

if ($isWindows) {
    $make = Join-Path $MinGWBin "mingw32-make.exe"
    if (-not (Test-Path -LiteralPath $make -PathType Leaf)) {
        throw "mingw32-make was not found at '$make'. Pass the MinGW bin directory with -MinGWBin."
    }
}
else {
    $make = "make"
}

function Convert-PngsToIco {
    param(
        [Parameter(Mandatory = $true)][string[]]$PngPaths,
        [Parameter(Mandatory = $true)][string]$OutputPath
    )

    $images = @()
    foreach ($path in $PngPaths) {
        $bytes = [System.IO.File]::ReadAllBytes($path)
        if ($bytes.Length -lt 24) {
            throw "Invalid PNG file: $path"
        }
        $w = [System.BitConverter]::ToInt32(@($bytes[19], $bytes[18], $bytes[17], $bytes[16]), 0)
        $h = [System.BitConverter]::ToInt32(@($bytes[23], $bytes[22], $bytes[21], $bytes[20]), 0)
        $images += [PSCustomObject]@{
            Width = $w
            Height = $h
            Bytes = $bytes
        }
    }
    $images = $images | Sort-Object Width

    $count = $images.Count
    $fs = [System.IO.File]::Create($OutputPath)
    $bw = New-Object System.IO.BinaryWriter($fs)
    try {
        $bw.Write([uint16]0)
        $bw.Write([uint16]1)
        $bw.Write([uint16]$count)

        $offset = 6 + 16 * $count
        foreach ($img in $images) {
            $bWidth = if ($img.Width -ge 256) { [byte]0 } else { [byte]$img.Width }
            $bHeight = if ($img.Height -ge 256) { [byte]0 } else { [byte]$img.Height }
            $bw.Write($bWidth)
            $bw.Write($bHeight)
            $bw.Write([byte]0)
            $bw.Write([byte]0)
            $bw.Write([uint16]1)
            $bw.Write([uint16]32)
            $bw.Write([uint32]$img.Bytes.Length)
            $bw.Write([uint32]$offset)
            $offset += $img.Bytes.Length
        }
        foreach ($img in $images) {
            $bw.Write($img.Bytes)
        }
    }
    finally {
        $bw.Close()
        $fs.Close()
    }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$imageDirectory = Join-Path (Join-Path $repositoryRoot "SeiSeeMp") "images"
$iconMappings = @(
    @{ Svg = "segyfile.svg"; Png = "segyfile.png" },
    @{ Svg = "segdfile.svg"; Png = "segdfile.png" },
    @{ Svg = "sufile.svg"; Png = "sufile.png" },
    @{ Svg = "cstfile.svg"; Png = "cstfile.png" },
    @{ Svg = "segyfile.svg"; Png = "segyfile.png" },
    @{ Svg = "segdfile.svg"; Png = "segdfile.png" },
    @{ Svg = "sufile.svg"; Png = "sufile.png" },
    @{ Svg = "cstfile.svg"; Png = "cstfile.png" }
)

$icoMappings = @(
    @{ Svg = "segyfile.svg"; Ico = "segyfile.ico" },
    @{ Svg = "segdfile.svg"; Ico = "segdfile.ico" },
    @{ Svg = "sufile.svg"; Ico = "sufile.ico" },
    @{ Svg = "cstfile.svg"; Ico = "cstfile.ico" }
)

$icoSizes = @(16, 24, 32, 48, 64, 128, 256)

foreach ($mapping in $iconMappings) {
    $svgPath = Join-Path $imageDirectory $mapping.Svg
    if (-not (Test-Path -LiteralPath $svgPath -PathType Leaf)) {
        throw "SVG source was not found: '$svgPath'."
    }
}

$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("seisee-svg-render-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

try {
    $rendererSource = @'
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QString>
#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 4) {
        std::fprintf(stderr, "Usage: svg-renderer <input.svg> <output.png> <size>\n");
        return 2;
    }

    bool sizeOk = false;
    const int size = QString::fromLocal8Bit(argv[3]).toInt(&sizeOk);
    if (!sizeOk || size <= 0) {
        std::fprintf(stderr, "Image size must be a positive integer.\n");
        return 2;
    }

    QSvgRenderer renderer(QString::fromLocal8Bit(argv[1]));
    if (!renderer.isValid()) {
        std::fprintf(stderr, "Unable to load SVG: %s\n", argv[1]);
        return 3;
    }

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    painter.end();

    if (!image.save(QString::fromLocal8Bit(argv[2]), "PNG")) {
        std::fprintf(stderr, "Unable to save PNG: %s\n", argv[2]);
        return 4;
    }
    return 0;
}
'@
    $projectFile = @'
QT += core gui svg
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
TARGET = svg-renderer
SOURCES += main.cpp
'@

    Set-Content -LiteralPath (Join-Path $temporaryDirectory "main.cpp") -Value $rendererSource -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $temporaryDirectory "svg-renderer.pro") -Value $projectFile -Encoding ASCII

    $previousPath = $env:PATH
    $pathEntries = @($QtBin, $(if ($isWindows) { $MinGWBin }), $previousPath) | Where-Object { $_ }
    $env:PATH = $pathEntries -join [System.IO.Path]::PathSeparator
    $previousPlatform = $env:QT_QPA_PLATFORM
    $env:QT_QPA_PLATFORM = "offscreen"

    Push-Location $temporaryDirectory
    try {
        & $qmake "svg-renderer.pro" "CONFIG+=release"
        if ($LASTEXITCODE -ne 0) {
            throw "qmake failed with exit code $LASTEXITCODE."
        }

        & $make "-j4"
        if ($LASTEXITCODE -ne 0) {
            throw "Building the SVG renderer failed with exit code $LASTEXITCODE."
        }

        $rendererName = if ($isWindows) { "svg-renderer.exe" } else { "svg-renderer" }
        $rendererExecutable = Join-Path (Join-Path $temporaryDirectory "release") $rendererName
        foreach ($mapping in $iconMappings) {
            $svgPath = Join-Path $imageDirectory $mapping.Svg
            $pngPath = Join-Path $imageDirectory $mapping.Png
            if (Test-Path -LiteralPath $svgPath -PathType Leaf) {
                & $rendererExecutable $svgPath $pngPath $Size
                if ($LASTEXITCODE -ne 0) {
                    throw "Rendering '$($mapping.Svg)' failed with exit code $LASTEXITCODE."
                }
                Write-Output "Generated $pngPath ($Size x $Size)"
            }
        }

        foreach ($icoMap in $icoMappings) {
            $svgPath = Join-Path $imageDirectory $icoMap.Svg
            $icoPath = Join-Path $imageDirectory $icoMap.Ico
            if (Test-Path -LiteralPath $svgPath -PathType Leaf) {
                $tempPngList = @()
                foreach ($sz in $icoSizes) {
                    $tempPng = Join-Path $temporaryDirectory "$($icoMap.Ico)_$sz.png"
                    & $rendererExecutable $svgPath $tempPng $sz
                    if ($LASTEXITCODE -ne 0) {
                        throw "Rendering '$($icoMap.Svg)' at $sz x $sz failed with exit code $LASTEXITCODE."
                    }
                    $tempPngList += $tempPng
                }
                Convert-PngsToIco -PngPaths $tempPngList -OutputPath $icoPath
                Write-Output "Generated $icoPath (Sizes: $($icoSizes -join ', '))"
            }
        }
    }
    finally {
        Pop-Location
        $env:PATH = $previousPath
        if ($null -eq $previousPlatform) {
            Remove-Item Env:QT_QPA_PLATFORM -ErrorAction SilentlyContinue
        }
        else {
            $env:QT_QPA_PLATFORM = $previousPlatform
        }
    }
}
finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
