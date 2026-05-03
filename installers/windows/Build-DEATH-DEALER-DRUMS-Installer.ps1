param(
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = 'Stop'

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Resolve-Path (Join-Path $scriptRoot "..\..")
$issPath = Join-Path $scriptRoot 'DEATH_DEALER_DRUMS_Installer.iss'

$vst3Path = Join-Path $projectRoot 'DELIVERABLES\Windows\VST3\DEATH DEALER DRUMS.vst3'
$standalonePath = Join-Path $projectRoot 'DELIVERABLES\Windows\Standalone\DEATH DEALER DRUMS.exe'

if (-not (Test-Path $vst3Path)) {
    throw "Missing VST3 deliverable: $vst3Path"
}

if (-not (Test-Path $standalonePath)) {
    throw "Missing standalone deliverable: $standalonePath"
}

$iscc = Get-Command iscc -ErrorAction SilentlyContinue
if ($null -eq $iscc) {
    throw "Inno Setup compiler (iscc) not found in PATH. Install Inno Setup 6 and run this script again."
}

Push-Location $scriptRoot
try {
    & $iscc.Source "/DAppVersion=$Version" $issPath
} finally {
    Pop-Location
}

Write-Host "Installer build complete." -ForegroundColor Green
Write-Host "Output folder: $scriptRoot" -ForegroundColor Cyan
