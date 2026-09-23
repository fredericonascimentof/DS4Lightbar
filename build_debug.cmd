@echo off
setlocal
set "PATH=F:\Qt\Tools\mingw1310_64\bin;%PATH%"

F:\Qt\Tools\CMake_64\bin\cmake.exe --preset qt-mingw-debug
if errorlevel 1 (
    pause
    exit /b 1
)

F:\Qt\Tools\CMake_64\bin\cmake.exe --build --preset qt-mingw-debug
if errorlevel 1 (
    pause
    exit /b 1
)

pause
