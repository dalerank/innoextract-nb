# Regression helper for Akhenaten innoextract builds
#
# Usage:
#   .\tools\regression_extract.ps1 -InnoextractPath <exe> -Installer <setup.exe> -OutDir <dir>
#
# Exit 0 if campaign.txt is found under OutDir after extract.

param(
    [Parameter(Mandatory = $true)][string]$InnoextractPath,
    [Parameter(Mandatory = $true)][string]$Installer,
    [Parameter(Mandatory = $true)][string]$OutDir
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $InnoextractPath)) {
    Write-Error "innoextract not found: $InnoextractPath"
    exit 1
}
if (-not (Test-Path $Installer)) {
    Write-Error "Installer not found: $Installer"
    exit 1
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Write-Host "Extracting $Installer -> $OutDir"
& $InnoextractPath -e -d $OutDir $Installer
if ($LASTEXITCODE -ne 0) {
    Write-Error "innoextract failed with exit code $LASTEXITCODE"
    exit 1
}

$campaign = Get-ChildItem -Path $OutDir -Recurse -Filter "campaign.txt" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $campaign) {
    Write-Error "campaign.txt not found under $OutDir"
    exit 1
}

Write-Host "OK: $($campaign.FullName)"
exit 0
