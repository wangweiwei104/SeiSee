@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
if not exist "%~dp0..\build" mkdir "%~dp0..\build"
cd /d "%~dp0..\build"
C:\ProgramData\anaconda3\Library\bin\qmake.exe ..\GxApps.pro "CONFIG+=release" "CONFIG-=debug" "QMAKE_CXXFLAGS+=/utf-8" "QMAKE_CXXFLAGS+=/FS"
if errorlevel 1 exit /b 1
nmake /NOLOGO > build.log 2>&1
exit /b %errorlevel%
