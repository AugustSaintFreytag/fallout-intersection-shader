# Dumps a RenderDoc capture to JSON lines via qrenderdoc's embedded Python.
# Usage: .\dump.ps1 "..\..\Captures\Capture.rdc" "out.jsonl"
param([string]$Capture, [string]$Out)
$env:RD_CAPTURE = (Resolve-Path $Capture).Path
$env:RD_OUT = [System.IO.Path]::GetFullPath($Out)
$p = Start-Process -FilePath "C:\Program Files\RenderDoc\qrenderdoc.exe" -ArgumentList "--python", "`"$PSScriptRoot\rd_dump.py`"" -PassThru
$p.WaitForExit()
Get-Content ($env:RD_OUT -replace '\.jsonl$', '.log')
