@echo off
rem PhyriadFG UI — lanza la UI usando el WebView2 FIJO local (carpeta de la app),
rem sin instalar nada en el sistema. El env-var apunta al runtime extraido del cab.
set "WEBVIEW2_BROWSER_EXECUTABLE_FOLDER=%~dp0webview2\Microsoft.WebView2.FixedVersionRuntime.149.0.4022.98.x64"
rem Endurecimiento bajo GPU saturada: la UI es un formulario simple y NO necesita GPU. Desactivar
rem la aceleracion GPU del WebView evita que su proceso GPU muera por device-loss/contencion cuando
rem el juego satura la GPU (alt-tab / cambio de modo) -> la UI sobrevive a la saturacion.
set "WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--disable-gpu --disable-gpu-compositing"
start "" "%~dp0src-tauri\target\release\ui.exe"
