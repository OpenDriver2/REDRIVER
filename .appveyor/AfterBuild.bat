@echo off
rem Packs each configuration: exe, pdb and the DLLs copied next to it by the post-build step.
setlocal

rem 7-Zip is in PATH on AppVeyor; fall back to the usual installation folder elsewhere
set "SEVENZIP=7z"
if exist "%ProgramFiles%\7-Zip\7z.exe" set "SEVENZIP=%ProgramFiles%\7-Zip\7z.exe"

for %%c in (Debug Release) do (
    cd /d "%project_folder%\bin\%%c" || exit /b 1
    if not exist Driver.exe (
        echo Driver.exe is missing in the %%c output
        exit /b 1
    )
    "%SEVENZIP%" a "OpenDriver1_%%c.zip" ".\*" >nul || exit /b 1
)
