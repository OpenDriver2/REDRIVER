@echo off
rem Generates the Visual Studio 2022 solution and builds Debug and Release (x86).
setlocal

cd /d "%project_folder%" || exit /b 1

rem Visual Studio is not always the same edition (Community/Professional/Enterprise),
rem so ask vswhere for the installation that is present on the image
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo Visual Studio not found
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul || exit /b 1

rem premake5.exe ships with the repository, no download needed
premake5.exe vs2022 || exit /b 1

rem nicer output in the AppVeyor log; the logger is absent when the script is run locally
set "MSBUILD_LOGGER=C:\Program Files\AppVeyor\BuildAgent\Appveyor.MSBuildLogger.dll"

for %%c in (Debug Release) do (
    if exist "%MSBUILD_LOGGER%" (
        msbuild build\OpenDriver1.sln /nologo /m /v:minimal /p:Configuration=%%c /p:Platform=Win32 ^
            /logger:"%MSBUILD_LOGGER%" || exit /b 1
    ) else (
        msbuild build\OpenDriver1.sln /nologo /m /v:minimal /p:Configuration=%%c /p:Platform=Win32 || exit /b 1
    )
)
