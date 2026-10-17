# Builds the plugin, then copies it into the MO2 mod folder.
#
# Usage: pwsh Plugin\build-and-deploy.ps1 [-ModPath <folder>] [-Ini]
# The mod folder comes from -ModPath, else from the first line of deploy-path.txt in the project root.
# deploy-path.txt is not versioned.
# INIs in the mod folder are only written if missing. -Ini overwrites them with the ones from Plugin\.

param(
    [string]$ModPath,
    [switch]$Ini
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

if (-not $ModPath) {
    $pathFile = Join-Path $root 'deploy-path.txt'
    if (-not (Test-Path $pathFile)) {
        throw "No mod folder given. Pass -ModPath or write the folder into $pathFile."
    }
    $ModPath = (Get-Content $pathFile -TotalCount 1).Trim()
}
if (-not (Test-Path $ModPath -PathType Container)) {
    throw "Mod folder not found: $ModPath"
}

& (Join-Path $PSScriptRoot 'build.ps1')

$source = Join-Path $root 'Build\Plugin\NVSE\Plugins'
$target = Join-Path $ModPath 'NVSE\Plugins'
New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item (Join-Path $source 'IntersectionBlend.dll'), (Join-Path $source 'IntersectionBlend.pdb') $target
Write-Host "Plugin deployed to $target"

# The installed INIs usually hold local test settings.
foreach ($name in 'IntersectionBlend.ini', 'IntersectionBlendForms.ini') {
    $iniTarget = Join-Path $target $name
    if ($Ini -or -not (Test-Path $iniTarget)) {
        Copy-Item (Join-Path $PSScriptRoot $name) $iniTarget
        Write-Host "Wrote $name"
    }
}
