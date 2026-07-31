@echo off
REM Copyright (c) Arduino s.r.l. and/or its affiliated companies
REM SPDX-License-Identifier: Apache-2.0

setlocal enabledelayedexpansion

set "ELF=%~1"
set "ADDR=%~2"
set "LINKSERVER="

for /f "delims=" %%D in ('dir /b /ad /o-n "C:\nxp\LinkServer_*" 2^>nul') do (
    if not defined LINKSERVER (
        if exist "C:\nxp\%%D\LinkServer.exe" (
            set "LINKSERVER=C:\nxp\%%D\LinkServer.exe"
        )
    )
)

if not defined LINKSERVER (
    where LinkServer.exe >nul 2>nul
    if !errorlevel! == 0 (
        set "LINKSERVER=LinkServer.exe"
    )
)

if not defined LINKSERVER (
    echo ERROR: LinkServer not found.
    exit /b 1
)

echo Using: !LINKSERVER!

if not "%ADDR%"=="" (
    "%LINKSERVER%" flash MCXN947:FRDM-MCXN947 load "%ELF%:%ADDR%"
) else (
    "%LINKSERVER%" flash MCXN947:FRDM-MCXN947 load "%ELF%"
)
