@echo off
rem Wrapper for compile.ps1, which compiles the shaders in parallel.
rem   compile.bat [-Variant all|vpt|parallax|parallax-optimized] [-Force]
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0compile.ps1" %*
