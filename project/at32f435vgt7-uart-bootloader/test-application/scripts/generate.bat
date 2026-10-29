@echo off
PolyBootGen.exe build -bin "%~dp0..\Debug\Exe\at32f435vgt7-blink.bin" -config "%~dp0config.xml" -out "%~dp0at32f435vgt7-blink.xbin"
