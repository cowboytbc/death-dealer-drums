param(
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path (Join-Path $scriptRoot "..\..")
$issPath = Join-Path $scriptRoot 'DEATH_DEALER_DRUMS_Installer.iss'
$logoPngPath = Join-Path $projectRoot 'LOGO.png'
$logoIcoPath = Join-Path $projectRoot 'LOGO.ico'

$vst3Path = Join-Path $projectRoot 'DELIVERABLES\Windows\VST3\DEATH DEALER DRUMS.vst3'
$standalonePath = Join-Path $projectRoot 'DELIVERABLES\Windows\Standalone\DEATH DEALER DRUMS.exe'

if (-not (Test-Path $vst3Path)) {
    throw "Missing VST3 deliverable: $vst3Path"
}

if (-not (Test-Path $standalonePath)) {
    throw "Missing standalone deliverable: $standalonePath"
}

if (-not (Test-Path $logoPngPath)) {
    throw "Missing logo source image: $logoPngPath"
}

# Windows desktop shortcuts require .ico/.exe icon resources.
# Convert LOGO.png -> LOGO.ico so installed shortcuts are guaranteed branded.
$needsIco = (-not (Test-Path $logoIcoPath)) -or ((Get-Item $logoIcoPath).LastWriteTimeUtc -lt (Get-Item $logoPngPath).LastWriteTimeUtc)
if ($needsIco) {
    $magickCmd = Get-Command magick -ErrorAction SilentlyContinue
    if ($null -ne $magickCmd) {
        # Generate true multi-size Windows icon from LOGO.png (crisper shell rendering)
        & $magickCmd.Source $logoPngPath `
            -background none `
            -gravity center `
            -resize 256x256 `
            -extent 256x256 `
            -define icon:auto-resize=256,192,128,96,64,48,40,32,24,20,16 `
            $logoIcoPath

        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $logoIcoPath)) {
            throw "Failed to generate LOGO.ico with ImageMagick"
        }
    }
    else {
        Add-Type -AssemblyName System.Drawing

        $sourceImage = $null
        $iconBitmap = $null
        $graphics = $null
        $icon = $null
        $hIcon = [IntPtr]::Zero
        $fileStream = $null
        try {
            $sourceImage = [System.Drawing.Image]::FromFile($logoPngPath)

            # Fallback path if ImageMagick is unavailable
            $iconBitmap = New-Object System.Drawing.Bitmap(256, 256, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            $graphics = [System.Drawing.Graphics]::FromImage($iconBitmap)
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

            $srcW = [double]$sourceImage.Width
            $srcH = [double]$sourceImage.Height
            $scale = [Math]::Min(256.0 / $srcW, 256.0 / $srcH)
            $dstW = [int][Math]::Round($srcW * $scale)
            $dstH = [int][Math]::Round($srcH * $scale)
            $dstX = [int][Math]::Floor((256 - $dstW) / 2)
            $dstY = [int][Math]::Floor((256 - $dstH) / 2)
            $graphics.DrawImage($sourceImage, $dstX, $dstY, $dstW, $dstH)

            $hIcon = $iconBitmap.GetHicon()
            $icon = [System.Drawing.Icon]::FromHandle($hIcon)
            $fileStream = [System.IO.File]::Open($logoIcoPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
            $icon.Save($fileStream)
        }
        finally {
            if ($fileStream -ne $null) { $fileStream.Dispose() }
            if ($icon -ne $null) { $icon.Dispose() }
            if ($graphics -ne $null) { $graphics.Dispose() }
            if ($iconBitmap -ne $null) { $iconBitmap.Dispose() }
            if ($sourceImage -ne $null) { $sourceImage.Dispose() }
        }
    }
}

$isccPath = $null
$isccCmd = Get-Command iscc -ErrorAction SilentlyContinue
if ($null -ne $isccCmd) {
    $isccPath = $isccCmd.Source
}

if ([string]::IsNullOrWhiteSpace($isccPath)) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
        'C:\Program Files\Inno Setup 6\ISCC.exe',
        'C:\Program Files (x86)\Inno Setup 5\ISCC.exe'
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $isccPath = $candidate
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($isccPath)) {
    throw "Inno Setup compiler (ISCC.exe) not found. Install Inno Setup 6 and run this script again."
}

Push-Location $scriptRoot
try {
    & $isccPath "/DAppVersion=$Version" $issPath
} finally {
    Pop-Location
}

Write-Host "Installer build complete." -ForegroundColor Green
Write-Host "Output folder: $scriptRoot" -ForegroundColor Cyan
