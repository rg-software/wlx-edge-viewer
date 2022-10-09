REM wxwidgets
set PROJECT_DEPS=webview2 wil

vcpkg install %PROJECT_DEPS% --triplet x86-windows
vcpkg install %PROJECT_DEPS% --triplet x64-windows
