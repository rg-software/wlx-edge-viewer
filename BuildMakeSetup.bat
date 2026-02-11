:: This script creates a release (setup) package
@echo off

setlocal
call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" x86
msbuild EdgeViewer.sln /t:Build /p:Configuration=Release;Platform=Win32;UseEnv=true
endlocal

setlocal
call "%VCINSTALLDIR%\Auxiliary\Build\vcvarsall.bat" x64
msbuild EdgeViewer.sln /t:Build /p:Configuration=Release;Platform=x64;UseEnv=true
endlocal

:::::::::::::::::::

del *.zip
rmdir /S /Q Build\Release
mkdir Build\Release
robocopy Resources\ Build\Release /S
copy Build\EdgeViewer_Win32_Release\EdgeViewer-Win32.dll Build\Release\EdgeViewer.wlx
copy Build\EdgeViewer_x64_Release\EdgeViewer-x64.dll Build\Release\EdgeViewer.wlx64
powershell.exe -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::CreateFromDirectory('Build\Release', 'Release-' + (get-date -Format yyyyMMdd) +'.zip'); }"

echo Done!

exit /b

