# Builds the plugin and shaders, then copies both into the MO2 mod folder.
#
# Usage: pwsh build-and-deploy.ps1 [-ModPath <folder>] [-Ini] [-SkipPlugin] [-SkipShaders] [-Force]
# The mod folder comes from -ModPath, else from the first line of deploy-path.txt next to this script.
# deploy-path.txt is not versioned.
# -Ini overwrites the INIs in the mod folder. See "Plugin\build-and-deploy.ps1".

param(
    [string]$ModPath,
    [switch]$Ini,
    [switch]$SkipPlugin,
    [switch]$SkipShaders,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if (-not $ModPath) {
    $pathFile = Join-Path $PSScriptRoot 'deploy-path.txt'
    if (-not (Test-Path $pathFile)) {
        throw "No mod folder given. Pass -ModPath or write the folder into $pathFile."
    }
    $ModPath = (Get-Content $pathFile -TotalCount 1).Trim()
}
if (-not (Test-Path $ModPath -PathType Container)) {
    throw "Mod folder not found: $ModPath"
}

if (-not $SkipPlugin) {
    & (Join-Path $PSScriptRoot 'Plugin\build-and-deploy.ps1') -ModPath $ModPath -Ini:$Ini
}

if (-not $SkipShaders) {
    & (Join-Path $PSScriptRoot 'build.ps1') -SkipPlugin -Force:$Force

    $target = Join-Path $ModPath 'Shaders\Loose\IntersectionBlend'
    New-Item -ItemType Directory -Force $target | Out-Null
    Copy-Item (Join-Path $PSScriptRoot 'Build\Plugin\Shaders\Loose\IntersectionBlend\*') $target -Recurse -Force
    Write-Host "Shaders deployed to $target"
}
