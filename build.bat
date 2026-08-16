@echo off
setlocal EnableExtensions

set "OUT_DIR=%~dp0build"
set "BUILD_MODE=%~1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

if /I not "%BUILD_MODE%"=="debug" if /I not "%BUILD_MODE%"=="release" (
  echo [vsbuild] Usage: %~nx0 [debug^|release]
  exit /b 1
)

where cl.exe >nul 2>nul && goto ready
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo [vsbuild] MSVC and vswhere.exe not found. Install Visual Studio Build Tools.
  exit /b 1
)

set "VCVARS="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
)

if not defined VCVARS (
  echo [vsbuild] Visual Studio with the C++ workload not found. Install "Desktop development with C++".
  exit /b 1
)

call "%VCVARS%" >nul
if not defined VCToolsInstallDir (
  echo [vsbuild] Failed to initialize MSVC x64 environment.
  exit /b 1
)

:ready

set "CL_FLAGS_COMMON=/nologo /std:c++20 /utf-8 /W4 /WX /EHsc- /GR- /DUNICODE /D_UNICODE /DFOXUI_PLATFORM_WINDOWS /Isrc /Ivendor"
set "LIBRARIES=user32.lib gdi32.lib ole32.lib imm32.lib d3d11.lib dxgi.lib dwrite.lib windowscodecs.lib shell32.lib dwmapi.lib"

if /I "%BUILD_MODE%"=="debug" set "CL_FLAGS=%CL_FLAGS_COMMON% /Od /Zi /MTd /DFOXUI_DEBUG"
if /I "%BUILD_MODE%"=="release" set "CL_FLAGS=%CL_FLAGS_COMMON% /O2 /MT /DNDEBUG"

if not exist "%OUT_DIR%" mkdir %OUT_DIR%

:: Building just the Library here
cl %CL_FLAGS% ^
   /c src\windows\foxui_win32.cpp ^
   /Fo"%OUT_DIR%\foxui.obj" /Fd"%OUT_DIR%\foxui.pdb" || exit /b 1

lib /nologo /out:"%OUT_DIR%\foxui.lib" "%OUT_DIR%\foxui.obj" || exit /b 1

:: Building the example here...
cl %CL_FLAGS% ^
   example\calc.cpp "%OUT_DIR%\foxui.lib" ^
   /Fo"%OUT_DIR%\foxui_calc.obj" /Fd"%OUT_DIR%\foxui_calc.pdb" ^
   /Fe"%OUT_DIR%\foxui_calc.exe" ^
   /link /pdb:"%OUT_DIR%\foxui_calc.pdb" /incremental:no ^
   /subsystem:windows /entry:mainCRTStartup %LIBRARIES% || exit /b 1

echo [build] Foxui %BUILD_MODE% build complete.
