:: This script creates a release (setup) package
@echo off
setlocal

call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" x86
msbuild EdgeViewer.sln /t:Build /p:Configuration=Release;Platform=Win32;UseEnv=true
call :package Release-32 EdgeViewer_Win32_Release EdgeViewer-Win32.dll EdgeViewer.wlx

call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" x64
msbuild EdgeViewer.sln /t:Build /p:Configuration=Release;Platform=x64;UseEnv=true
call :package Release-64 EdgeViewer_x64_Release EdgeViewer-x64.dll EdgeViewer.wlx64

goto :eof

:::::::::::::::::::

:package
:: args: out-dir EdgeViewer-dll-dir Edge-Viewer-dll-name out-wlx-name
del %1.zip
rmdir /S /Q Build\%1
mkdir Build\%1
robocopy Resources\ Build\%1 /S
copy Build\%2\%3 Build\%1\%4
copy Build\%2\WebView2Loader.dll Build\%1\WebView2Loader.dll
powershell.exe -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::CreateFromDirectory('Build\%1', '%1.zip'); }"
echo Resulting file %1.zip created!
exit /b

