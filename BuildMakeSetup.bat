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
rmdir /S /Q winbuild\Release
mkdir winbuild\Release
robocopy Resources\ winbuild\Release /S
copy winbuild\EdgeViewer_Win32_Release\EdgeViewer-Win32.dll winbuild\Release\EdgeViewer.wlx
copy winbuild\EdgeViewer_x64_Release\EdgeViewer-x64.dll winbuild\Release\EdgeViewer.wlx64
powershell.exe -nologo -noprofile -command "& { Add-Type -A 'System.IO.Compression.FileSystem'; [IO.Compression.ZipFile]::CreateFromDirectory('winbuild\Release', 'Release-' + (get-date -Format yyyyMMdd) + '-Win.zip'); }"

echo Done!

exit /b

