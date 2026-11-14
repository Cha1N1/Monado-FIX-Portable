@echo off
setlocal EnableExtensions
cd /d "%~dp0\.."
set "OUT=%CD%\gpu-diagnostics"
if not exist "%OUT%" mkdir "%OUT%"
echo Collecting Windows and Vulkan adapter information...
(systeminfo) > "%OUT%\systeminfo.txt" 2>&1
(powershell -NoProfile -Command "Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,DriverDate,AdapterRAM,VideoProcessor | Format-List") > "%OUT%\display-adapters.txt" 2>&1
where vulkaninfo.exe >nul 2>nul
if not errorlevel 1 (
  vulkaninfo.exe --summary > "%OUT%\vulkaninfo-summary.txt" 2>&1
) else (
  echo vulkaninfo.exe was not found in PATH. Install Vulkan SDK or add its Bin directory to PATH. > "%OUT%\vulkaninfo-summary.txt"
)
if exist xcde.ini copy /y xcde.ini "%OUT%\xcde.ini" >nul
if exist xcde_env.ini copy /y xcde_env.ini "%OUT%\xcde_env.ini" >nul
if exist build\validation\gpu_diagnostics.exe (
  build\validation\gpu_diagnostics.exe > "%OUT%\monado-adapters.txt" 2>&1
)
(
  echo GPU diagnostics collected: %DATE% %TIME%
  echo Attach this folder together with the game's runtime log.
  echo Do not include game dump files, keys, saves, or other copyrighted/protected data.
) > "%OUT%\README.txt"
echo.
echo Diagnostics saved to: %OUT%
echo Please also attach the runtime log generated during the test session.
pause
endlocal
