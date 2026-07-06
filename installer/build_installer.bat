@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul
title QT-AGV Installer Builder

set "ROOT_DIR=%~dp0.."
set "QT_DIR=F:\Qt\6.11.0\mingw_64"
set "MINGW_DIR=F:\Qt\Tools\mingw1310_64"
set "CMAKE_EXE=F:\cmake\bin\cmake.exe"
set "BUILD_DIR=%ROOT_DIR%\build-release"
set "DIST_DIR=%~dp0dist"
set "OUTPUT_DIR=%~dp0output"
set "APP_VERSION=1.0.0"

set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"

echo ============================================
echo   QT-AGV Installer Builder
echo ============================================
echo.

echo [1/5] Configuring CMake (Release)...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
"%CMAKE_EXE%" -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_DIR%" -DCMAKE_C_COMPILER="%MINGW_DIR%\bin\gcc.exe" -DCMAKE_CXX_COMPILER="%MINGW_DIR%\bin\c++.exe" -DCMAKE_MAKE_PROGRAM="%MINGW_DIR%\bin\mingw32-make.exe"
if errorlevel 1 goto :fail

echo.
echo [2/5] Building Release...
if exist "%BUILD_DIR%\QT-AGV_autogen\timestamp" del /f /q "%BUILD_DIR%\QT-AGV_autogen\timestamp"
"%MINGW_DIR%\bin\mingw32-make.exe" -C "%BUILD_DIR%" QT-AGV_autogen 2>nul
"%MINGW_DIR%\bin\mingw32-make.exe" -C "%BUILD_DIR%" -j4
if errorlevel 1 goto :fail

echo.
echo [3/5] Collecting Qt runtime deps (windeployqt)...
if exist "%DIST_DIR%" rmdir /s /q "%DIST_DIR%"
mkdir "%DIST_DIR%"
copy /y "%BUILD_DIR%\QT-AGV.exe" "%DIST_DIR%\" >nul
"%QT_DIR%\bin\windeployqt6.exe" --no-translations "%DIST_DIR%\QT-AGV.exe"
if errorlevel 1 goto :fail

echo.
echo [4/5] Checking for Inno Setup...
set "ISCC="
if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"

if not defined ISCC (
    echo   Inno Setup not found, downloading and installing silently...
    set "ISSETUP_TMP=%TEMP%\innosetup-installer.exe"
    powershell -NoProfile -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -UseBasicParsing -Uri 'https://jrsoftware.org/download.php/is.exe' -OutFile '!ISSETUP_TMP!' | Out-Null"
    if errorlevel 1 (
        echo [ERROR] Failed to download Inno Setup. Install manually: https://jrsoftware.org/isdl.php
        goto :fail
    )
    "!ISSETUP_TMP!" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-
    if errorlevel 1 goto :fail

    if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
)

if not defined ISCC (
    echo [ERROR] Could not find or install Inno Setup. Install manually: https://jrsoftware.org/isdl.php
    goto :fail
)
echo   Using: %ISCC%

echo.
echo [5/5] Building installer...
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"
"%ISCC%" /DMyAppVersion=%APP_VERSION% "%~dp0installer.iss"
if errorlevel 1 goto :fail

echo.
echo ============================================
echo   Installer built successfully!
echo   Output: %OUTPUT_DIR%
echo ============================================
explorer "%OUTPUT_DIR%"
pause
exit /b 0

:fail
echo.
echo [FAILED] See log above for details.
pause
exit /b 1
