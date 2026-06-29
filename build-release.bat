@echo off
rem PhyriadFG — release build -> build-release\phyriad_fg.exe (distributable, no debug DLLs)
setlocal

rem ── Locate Visual Studio via vswhere ────────────────────────────────────────
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo ERROR: vswhere.exe not found. Install Visual Studio 2022 Build Tools:
    echo        https://aka.ms/vs/17/release/vs_BuildTools.exe
    exit /b 1
)
for /f "usebackq delims=" %%i in (
    `"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`
) do set "VS_PATH=%%i"
if not defined VS_PATH (
    echo ERROR: No Visual Studio installation with C++ tools found.
    exit /b 1
)
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

rem ── Vulkan SDK ───────────────────────────────────────────────────────────────
if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK is not set. Install the Vulkan SDK from:
    echo        https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

cmake -S "%~dp0." -B "%~dp0build-release" -G Ninja -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DVulkan_INCLUDE_DIR="%VULKAN_SDK%/Include" ^
    -DVulkan_LIBRARY="%VULKAN_SDK%/Lib/vulkan-1.lib" || exit /b 1
cmake --build "%~dp0build-release" || exit /b 1

echo.
echo === build OK: %~dp0build-release\phyriad_fg.exe ===
