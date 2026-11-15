@echo off
if defined OPENCRAFT_DATA_DIR goto data
if not exist "%~dp0Uninstall.exe" goto portable
set "OPENCRAFT_DATA_DIR=%LOCALAPPDATA%\OpenCraft"
:data
if not exist "%OPENCRAFT_DATA_DIR%" (
    mkdir "%OPENCRAFT_DATA_DIR%"
    if errorlevel 1 exit /b 1
)
for %%D in (mods resources) do if not exist "%OPENCRAFT_DATA_DIR%\%%D" (
    mklink /J "%OPENCRAFT_DATA_DIR%\%%D" "%~dp0%%D"
    if errorlevel 1 exit /b 1
)
cd /d "%OPENCRAFT_DATA_DIR%"
if errorlevel 1 exit /b 1
"%~dp0opencraft.exe" %*
exit /b %errorlevel%
:portable
cd /d "%~dp0"
opencraft.exe %*
