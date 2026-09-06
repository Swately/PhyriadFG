@echo off
rem PhyriadFG - release build -> build-release\phyriad_fg.exe (distributable, no debug DLLs)
rem
rem ASCII-ONLY BY CONTRACT: cmd.exe seeks this file by byte offset while decoding it as
rem characters, so any multi-byte (UTF-8) character - even inside a rem - desynchronizes the
rem read under codepage 65001 and chops the lines that follow. Keep every byte < 0x80.
setlocal

rem -- Locate Visual Studio via vswhere ----------------------------------------
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

rem -- Vulkan SDK ---------------------------------------------------------------
if not defined VULKAN_SDK (
    echo ERROR: VULKAN_SDK is not set. Install the Vulkan SDK from:
    echo        https://vulkan.lunarg.com/sdk/home
    exit /b 1
)

rem -- Pin the MSVC linker ------------------------------------------------------
rem A MinGW toolchain on PATH (WinLibs, Git) makes CMake resolve the linker to its
rem ld.exe, which then chokes on every MSVC flag (/nologo, /out:, *.lib). vcvars64
rem puts MSVC's link.exe first, so take the first hit and hand it to CMake outright.
for /f "delims=" %%i in ('where link.exe 2^>nul') do (
    if not defined MSVC_LINK set "MSVC_LINK=%%i"
)
if not defined MSVC_LINK (
    echo ERROR: link.exe not found after vcvars64 - check the VC++ toolset install.
    exit /b 1
)

cmake -S "%~dp0." -B "%~dp0build-release" -G Ninja -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_LINKER="%MSVC_LINK%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DVulkan_INCLUDE_DIR="%VULKAN_SDK%/Include" ^
    -DVulkan_LIBRARY="%VULKAN_SDK%/Lib/vulkan-1.lib" || exit /b 1
cmake --build "%~dp0build-release" || exit /b 1

echo.
echo === build OK: %~dp0build-release\phyriad_fg.exe ===

rem 4.3 (2026-09-06): the tests run here. Before 4.3 there was no enable_testing()/add_test() at all and the two
rem test binaries were built by every build and run by nothing. They cost ~1.5 s. The binary above is already
rem written, so a red suite blocks nothing -- it withdraws the claim that this build is good.
ctest --test-dir "%~dp0build-release" --output-on-failure
if errorlevel 1 (
    echo.
    echo === TESTS FAILED -- the binary was built, but do not trust it. Run: tools\run_tests.bat ===
    exit /b 1
)
echo === tests OK ===
rem Made with my soul - Swately <3
