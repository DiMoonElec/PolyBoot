@echo off
setlocal
set "POLYBOOT_GENERATOR=PolyBootGen.exe"
if not "%~1"=="" set "POLYBOOT_GENERATOR=%~1"
"%POLYBOOT_GENERATOR%" build-v2 -config "%~dp0complex.xml"
exit /b %errorlevel%
