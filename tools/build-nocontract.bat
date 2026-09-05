@echo off
rem tools\build-nocontract.bat ON|OFF -- the NoContraction EXPERIMENT build (ASCII only by contract).
rem build-release.bat's toolchain setup with -DPFG_NOCONTRACT=%1 on the configure line, so the CMake switch is set
rem INSIDE the vcvars environment (a configure outside it fails at the compiler test). ON = every .spv decorated
rem NoContraction inside the glslc rules (tools\spv_nocontract.py) -- never ship that binary; OFF restores the
rem product build (verify: the .spv md5s return to their pre-experiment values). Used by records\R3_GATE.md s4.
rem Made with my soul - Swately <3
setlocal
if "%~1"=="" ( echo usage: build-nocontract.bat ON^|OFF & exit /b 1 )
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo ERROR: vswhere.exe not found & exit /b 1 )
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%i"
if not defined VS_PATH ( echo ERROR: no VS C++ tools & exit /b 1 )
call "%VS_PATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VULKAN_SDK ( echo ERROR: VULKAN_SDK not set & exit /b 1 )
for /f "delims=" %%i in ('where link.exe 2^>nul') do ( if not defined MSVC_LINK set "MSVC_LINK=%%i" )
if not defined MSVC_LINK ( echo ERROR: link.exe not found & exit /b 1 )
set "SRC=%~dp0.."
set "BLD=%~dp0..\build-release"
cmake -S "%SRC%" -B "%BLD%" -G Ninja -DCMAKE_CXX_COMPILER=cl ^
    -DCMAKE_LINKER="%MSVC_LINK%" ^
    -DCMAKE_BUILD_TYPE=Release -DPFG_NOCONTRACT=%1 ^
    -DVulkan_INCLUDE_DIR="%VULKAN_SDK%/Include" ^
    -DVulkan_LIBRARY="%VULKAN_SDK%/Lib/vulkan-1.lib" || exit /b 1
cmake --build "%BLD%" || exit /b 1
echo === build OK (PFG_NOCONTRACT=%1): %BLD%\phyriad_fg.exe ===
