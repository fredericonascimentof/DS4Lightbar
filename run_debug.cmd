@echo off
setlocal
set "PATH=%~dp0build\qt-mingw-debug;F:\Qt\Tools\mingw1310_64\bin;%PATH%"
start "" "%~dp0build\qt-mingw-debug\DS4Lightbar.exe"
