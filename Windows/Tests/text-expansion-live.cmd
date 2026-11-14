@echo off
setlocal
rem Windows Phase 3 live test: install a temporary text-expansions.json, restart Vikey, type through SendInput,
rem then restore the developer's previous file. Intended for the Windows dev VM.
set "TXDIR=%LOCALAPPDATA%\Vikey"
set "TX=%TXDIR%\text-expansions.json"
set "BAK=%TEMP%\vikey-text-expansions-live-test.bak"
if not exist "%TXDIR%" mkdir "%TXDIR%"
if exist "%BAK%" del /q "%BAK%"
if exist "%TX%" copy /y "%TX%" "%BAK%" >nul
copy /y "%~dp0text-expansions.json" "%TX%" >nul

taskkill /IM Vikey.exe /F >nul 2>&1
start "" C:\vk\build\App\Vikey.exe
timeout /t 3 /nobreak >nul
C:\vk\build\App\vikey_livetest.exe C:\vk\src\Windows\Tests\text-expansion-cases.tsv
set "RC=%ERRORLEVEL%"

taskkill /IM Vikey.exe /F >nul 2>&1
if exist "%BAK%" (
  copy /y "%BAK%" "%TX%" >nul
  del /q "%BAK%"
) else (
  del /q "%TX%" >nul 2>&1
)
start "" C:\vk\build\App\Vikey.exe
exit /b %RC%
