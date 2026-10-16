# Builds the plugin and compiles the shaders into Build\Plugin.
#
# Usage: pwsh build.ps1 [-SkipPlugin] [-SkipShaders] [-Force]
# -Force rebuilds every shader. The plugin build is incremental either way.
# Needs PowerShell 7.

param(
    [switch]$SkipPlugin,
    [switch]$SkipShaders,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if (-not $SkipPlugin) {
    & (Join-Path $PSScriptRoot 'Plugin\build.ps1')
}

if (-not $SkipShaders) {
    # The compile script only sets an exit code when it fails.
    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot 'Shaders\compile.ps1') -Force:$Force
    if ($global:LASTEXITCODE -ne 0) {
        throw "Shader compile failed (exit code $LASTEXITCODE)."
    }
}
