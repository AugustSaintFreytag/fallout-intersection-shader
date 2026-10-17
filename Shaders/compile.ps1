# Compiles all shaders in parallel, one fxc process per shader. fxc is single-threaded.
#   Build\Plugin\Shaders\Loose\IntersectionBlend      plugin shaders
#   Build\Plugin\Shaders\Loose\IntersectionBlend\Ground\<variant>      ground mode pixel shaders, one set per terrain shader mod
#
# Usage: pwsh Shaders\compile.ps1 [-Variant all|vpt|parallax|parallax-optimized] [-Force]
# Shaders are skipped when their output is newer than every file under Shaders\. -Force rebuilds them all.
# The FXC environment variable selects a specific fxc.exe. Default is the newest one from the Windows SDK.

param(
    [ValidateSet('all', 'vpt', 'parallax', 'parallax-optimized')]
    [string]$Variant = 'all',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$shaders = $PSScriptRoot

$fxc = $env:FXC
if (-not $fxc) {
    $fxc = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin\*\x64\fxc.exe' -ErrorAction SilentlyContinue |
        Sort-Object { [version]$_.Directory.Parent.Name } -ErrorAction SilentlyContinue |
        Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $fxc -or -not (Test-Path $fxc)) {
    throw 'fxc.exe not found. Install the Windows SDK or set FXC.'
}

$jobs = [System.Collections.Generic.List[object]]::new()

function Add-Shader([string]$ShaderProfile, [string]$Source, [string]$Output, [string[]]$Defines, [string[]]$Flags = @()) {
    $jobs.Add([pscustomobject]@{
        Profile = $ShaderProfile
        Source = Join-Path $shaders $Source
        Output = Join-Path $root $Output
        Defines = $Defines
        Flags = $Flags
    })
}

$pluginOut = 'Build\Plugin\Shaders\Loose\IntersectionBlend'
Add-Shader 'vs_3_0' 'DebugViews\DebugDepth.hlsl' "$pluginOut\DebugDepth.vso" @('VS')
Add-Shader 'ps_3_0' 'DebugViews\DebugDepth.hlsl' "$pluginOut\DebugDepth.pso" @('PS')
Add-Shader 'vs_3_0' 'Ground\GroundBuffer.hlsl' "$pluginOut\GroundBuffer.vso" @('VS')
Add-Shader 'ps_3_0' 'Ground\GroundBuffer.hlsl' "$pluginOut\GroundBuffer.pso" @('PS')
Add-Shader 'vs_3_0' 'Ground\GroundQuad.hlsl' "$pluginOut\GroundQuad.vso" @()
Add-Shader 'ps_3_0' 'Ground\GroundField.hlsl' "$pluginOut\GroundField.pso" @()
Add-Shader 'ps_3_0' 'Toppers\TopperBlend.hlsl' "$pluginOut\TopperCopy.pso" @('COPY')
Add-Shader 'ps_3_0' 'Toppers\TopperBlend.hlsl' "$pluginOut\TopperComposite.pso" @('COMPOSITE')

# Ground mode pixel shaders: 7 texture counts x (no point lights, 6, 12, 24 point lights), one set per terrain shader mod.
# File name T<texture count>L<point lights>.pso. The plugin picks the set that matches the installed terrain shaders.
$groundVariants = @(
    @{ Key = 'vpt'; Folder = 'VPT'; Source = 'Terrain\VPT\GroundTemplate.hlsl'; Defines = @() }
    @{ Key = 'parallax'; Folder = 'Parallax'; Source = 'Terrain\Parallax\GroundTemplate.hlsl'; Defines = @() }
    @{ Key = 'parallax-optimized'; Folder = 'ParallaxOptimized'; Source = 'Terrain\Parallax\GroundTemplate.hlsl'; Defines = @('MAX_STEPS=8', 'MAX_DISTANCE=1024') }
)
$pointLights = @(0, 6, 12, 24)

foreach ($ground in $groundVariants) {
    if ($Variant -ne 'all' -and $Variant -ne $ground.Key) {
        continue
    }
    foreach ($textures in 1..7) {
        foreach ($lights in $pointLights) {
            $defines = @("TEX_COUNT=$textures") + $ground.Defines
            if ($lights) {
                $defines += "NUM_PT_LIGHTS=$lights"
            }
            Add-Shader 'ps_3_0' $ground.Source "$pluginOut\Ground\$($ground.Folder)\T$($textures)L$lights.pso" $defines
        }
    }
}

$missing = @($jobs.Source | Sort-Object -Unique | Where-Object { -not (Test-Path $_) })
if ($missing.Count -gt 0) {
    throw "Shader sources not found: $($missing -join ', ')"
}

# Every shader includes shared files. Any change under Shaders\ invalidates all outputs.
$newestSource = (Get-ChildItem $shaders -Recurse -Include *.hlsl, *.hlsli | Measure-Object LastWriteTimeUtc -Maximum).Maximum
$pending = @($jobs | Where-Object { $Force -or -not (Test-Path $_.Output) -or (Get-Item $_.Output).LastWriteTimeUtc -lt $newestSource })

if ($pending.Count -eq 0) {
    Write-Host "All $($jobs.Count) shaders are up to date."
    exit 0
}

foreach ($directory in $pending.Output | Split-Path -Parent | Sort-Object -Unique) {
    New-Item -ItemType Directory -Force $directory | Out-Null
}

Write-Host "Compiling $($pending.Count) of $($jobs.Count) shaders with $fxc"
$timer = [System.Diagnostics.Stopwatch]::StartNew()

$results = $pending | ForEach-Object -ThrottleLimit ([Environment]::ProcessorCount) -Parallel {
    $arguments = @('/nologo', '/T', $_.Profile, '/E', 'main') + $_.Flags + ($_.Defines | ForEach-Object { "/D$_" }) + @('/Fo', $_.Output, $_.Source)
    $log = & $using:fxc @arguments 2>&1 | Out-String
    [pscustomobject]@{ Output = $_.Output; ExitCode = $LASTEXITCODE; Log = $log }
}

$failed = @($results | Where-Object ExitCode -ne 0)
foreach ($result in $failed) {
    Write-Host "FAILED: $($result.Output)" -ForegroundColor Red
    Write-Host $result.Log
}

$warnings = @($results | Where-Object { $_.ExitCode -eq 0 -and $_.Log -match 'warning' })
foreach ($result in $warnings) {
    Write-Host "Warnings: $($result.Output)" -ForegroundColor Yellow
    Write-Host $result.Log
}

Write-Host ("{0} compiled, {1} failed in {2:N1} s" -f ($results.Count - $failed.Count), $failed.Count, $timer.Elapsed.TotalSeconds)
if ($failed.Count -gt 0) {
    exit 1
}
