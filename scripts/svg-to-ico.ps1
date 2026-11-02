# Convert configured SVG files to multi-resolution Windows ICO files with ImageMagick.
# Add source/output pairs to $IconMappings and adjust $IcoSizes when needed.

$ErrorActionPreference = "Stop"

$MagickCommand = "magick"
$IconMappings = @(
    @{ Svg = "SeiSeeMp.svg"; Ico = "SeiSeeMp.ico" },
    @{ Svg = "SeiSeeSetup.svg"; Ico = "SeiSeeSetup.ico" },
    @{ Svg = "segyfile.svg"; Ico = "segyfile.ico" },
    @{ Svg = "segdfile.svg"; Ico = "segdfile.ico" },
    @{ Svg = "sufile.svg"; Ico = "sufile.ico" },
    @{ Svg = "cstfile.svg"; Ico = "cstfile.ico" }
)

# Covers common Windows shell/icon sizes, including high-DPI and 256x256.
$IcoSizes = @(16, 20, 24, 28, 32, 36, 40, 44, 48, 60, 64, 72, 80, 96, 128, 256)
$RasterSize = 256
$SvgDensity = 768

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$imageDirectory = Join-Path (Join-Path $repositoryRoot "SeiSeeMp") "images"
$magickPath = (Get-Command -Name $MagickCommand -CommandType Application -ErrorAction Stop).Source

if ($IconMappings.Count -eq 0) {
    throw "Add at least one SVG/ICO pair to `$IconMappings."
}
if ($IcoSizes.Count -eq 0 -or ($IcoSizes | Where-Object { $_ -lt 1 -or $_ -gt 256 }).Count -gt 0) {
    throw "ICO sizes must be between 1 and 256 pixels."
}
if ($IcoSizes.Count -gt 16) {
    throw "ImageMagick includes at most 16 sizes in the generated ICO."
}
$uniqueSizeCount = @($IcoSizes | Select-Object -Unique).Count
if ($uniqueSizeCount -ne $IcoSizes.Count) {
    throw "ICO sizes must not contain duplicates."
}

$temporaryDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("seisee-ico-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $temporaryDirectory | Out-Null

try {
    foreach ($mapping in $IconMappings) {
        $svgPath = Join-Path $imageDirectory $mapping.Svg
        $icoPath = Join-Path $imageDirectory $mapping.Ico
        if (-not (Test-Path -LiteralPath $svgPath -PathType Leaf)) {
            throw "SVG source was not found: '$svgPath'."
        }

        $pngPath = Join-Path $temporaryDirectory ([System.IO.Path]::GetFileNameWithoutExtension($mapping.Ico) + ".png")
        & $magickPath -background none -density $SvgDensity $svgPath -resize "${RasterSize}x${RasterSize}" -gravity center -extent "${RasterSize}x${RasterSize}" "PNG:$pngPath"
        if ($LASTEXITCODE -ne 0) {
            throw "Rasterizing '$($mapping.Svg)' failed with exit code $LASTEXITCODE."
        }

        $sizeList = $IcoSizes -join ","
        & $magickPath $pngPath -define "icon:auto-resize=$sizeList" $icoPath
        if ($LASTEXITCODE -ne 0) {
            throw "Generating '$($mapping.Ico)' failed with exit code $LASTEXITCODE."
        }

        Write-Output "Generated $icoPath (Sizes: $sizeList)"
    }
}
finally {
    Remove-Item -LiteralPath $temporaryDirectory -Recurse -Force
}
