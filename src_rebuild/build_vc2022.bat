@echo off
rem Command line build: build_vc2022.bat [Debug|Release]
setlocal
rem English compiler/linker messages (if the English language pack is installed)
set VSLANG=1033
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set VSDIR=%%i
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul

cd /d %~dp0
"%~dp0premake5.exe" vs2022 >nul
msbuild build\OpenDriver1.sln /nologo /m /v:minimal /p:Configuration=%CONFIG% /p:Platform=Win32
