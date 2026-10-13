# Compiles all shaders in parallel (fxc is single-threaded, so one process per shader).
#   Build\Plugin\Shaders\Loose\IntersectionBlend      plugin shaders
#   Build\Terrain\<variant>\Shaders\Loose      terrain pixel shaders with the overlay mode, one set per terrain shader mod
#
# Usage: pwsh Shaders\compile.ps1 [-Variant all|vpt|parallax|parallax-optimized] [-Force]
# Shaders are skipped when their output is newer than every file under Shaders\. -Force rebuilds them all.
# Set the FXC environment variable to use a specific fxc.exe; otherwise the newest Windows SDK one is used.

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

function Add-Shader([string]$ShaderProfile, [string]$Source, [string]$Output, [string[]]$Defines) {
    $jobs.Add([pscustomobject]@{
        Profile = $ShaderProfile
        Source  = Join-Path $shaders $Source
        Output  = Join-Path $root $Output
        Defines = $Defines
    })
}

$pluginOut = 'Build\Plugin\Shaders\Loose\IntersectionBlend'
Add-Shader 'vs_3_0' 'DebugViews\DebugDepth.hlsl' "$pluginOut\DebugDepth.vso" @('VS')
Add-Shader 'ps_3_0' 'DebugViews\DebugDepth.hlsl' "$pluginOut\DebugDepth.pso" @('PS')

# Terrain pixel shaders SLS2092-SLS2146: 7 texture counts x (no point lights, 6, 12, 24 point lights).
# File number = 2092 + (texture count - 1) * 8 + light variant * 2, as in Vanilla Plus Terrain.
$terrainVariants = @(
    @{ Key = 'vpt';                Name = 'Vanilla Plus Terrain';         Source = 'Terrain\VPT\TerrainTemplate.hlsl';      Defines = @() }
    @{ Key = 'parallax';           Name = 'Parallax Shaders';             Source = 'Terrain\Parallax\TerrainTemplate.hlsl'; Defines = @() }
    @{ Key = 'parallax-optimized'; Name = 'Parallax Shaders (Optimized)'; Source = 'Terrain\Parallax\TerrainTemplate.hlsl'; Defines = @('MAX_STEPS=8', 'MAX_DISTANCE=1024') }
)
$pointLights = @($null, 6, 12, 24)

foreach ($terrain in $terrainVariants) {
    if ($Variant -ne 'all' -and $Variant -ne $terrain.Key) {
        continue
    }
    foreach ($textures in 1..7) {
        foreach ($lightVariant in 0..3) {
            $number = 2092 + ($textures - 1) * 8 + $lightVariant * 2
            $defines = @('PS', "TEX_COUNT=$textures") + $terrain.Defines
            if ($pointLights[$lightVariant]) {
                $defines += "NUM_PT_LIGHTS=$($pointLights[$lightVariant])"
            }
            Add-Shader 'ps_3_0' $terrain.Source "Build\Terrain\$($terrain.Name)\Shaders\Loose\SLS$number.pso" $defines
        }
    }
}

$missing = @($jobs.Source | Sort-Object -Unique | Where-Object { -not (Test-Path $_) })
if ($missing.Count -gt 0) {
    throw "Shader sources not found: $($missing -join ', ')"
}

# Every shader includes shared files, so any change under Shaders\ invalidates all outputs.
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
    $arguments = @('/nologo', '/T', $_.Profile, '/E', 'main') + ($_.Defines | ForEach-Object { "/D$_" }) + @('/Fo', $_.Output, $_.Source)
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
