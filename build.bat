@echo off
setlocal

set OUT_DIR=%~dp0build
set "BUILD_MODE=%~1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

where cl.exe >nul 2>nul
if errorlevel 1 (
  set "VSLOC=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

  if not exist "!VSLOC!" (
    echo [vsbuild] MSVC and vswhere.exe not found. Install Visual Studio Build Tools.
    exit /b 1
  )

  for /f "usebackq tokens=*" %%i in (`"!VSLOC!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
  )
)

where cl.exe >nul 2>nul || (
  echo [vsbuild] Failed to initialize MSVC x64 environment.
  exit /b 1
)

set "CL_FLAGS_COMMON=/nologo /std:c++20 /utf-8 /W4 /WX /EHsc- /GR- /DUNICODE /D_UNICODE /DFOXUI_PLATFORM_WINDOWS /Isrc /Ivendor"
set "LIBRARIES=user32.lib gdi32.lib ole32.lib imm32.lib d3d11.lib dxgi.lib d2d1.lib dwrite.lib windowscodecs.lib shell32.lib"

if /I "%BUILD_MODE%"=="debug" set "CL_FLAGS=%CL_FLAGS_COMMON% /Od /Zi /MTd /DFOXUI_DEBUG"
if /I "%BUILD_MODE%"=="release" set "CL_FLAGS=%CL_FLAGS_COMMON% /O2 /MT /DNDEBUG"

if not exists "%OUT_DIR%" mkdir %OUT_DIR%

:: Building just the Library here
cl %CL_FLAGS% ^
   /c src\foxui.cpp ^
   /Fo"%OUT_DIR\foxui.obj" /Fd"%OUT_DIR%\foxui.pdb" || exit /b 1

lib /nologo /out:"%OUT_DIR%\foxui.lib" "%OUT_DIR\foxui.obj" || exit /b 1

echo [build] Foxui %BUILD_MODE% build complete.
