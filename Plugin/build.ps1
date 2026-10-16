# Builds the plugin (Release|x86) into Build\Plugin\NVSE\Plugins.
#
# Usage: pwsh Plugin\build.ps1
# Needs Visual Studio 2022 with the C++ workload.

$ErrorActionPreference = 'Stop'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw 'vswhere.exe not found. Install Visual Studio 2022.'
}
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild.exe not found.'
}

# The post-build step copies the DLL, PDB and both INIs to Build\Plugin\NVSE\Plugins.
# MSBuild only prints errors.
& $msbuild (Join-Path $PSScriptRoot 'IntersectionBlend.sln') /p:Configuration=Release /p:Platform=x86 /m /v:quiet /clp:ErrorsOnly /nologo
if ($LASTEXITCODE -ne 0) {
    throw "Plugin build failed (exit code $LASTEXITCODE)."
}
Write-Host 'Build successful.'
