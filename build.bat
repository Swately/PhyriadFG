@echo off
rem PhyriadFG — un solo comando de build: configura + compila phyriad_fg.exe en build\.
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined VULKAN_SDK set "VULKAN_SDK=G:\VulkanSDK"
cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_CXX_COMPILER=cl || exit /b 1
cmake --build "%~dp0build" || exit /b 1
echo.
echo === build OK: %~dp0build\phyriad_fg.exe ===
