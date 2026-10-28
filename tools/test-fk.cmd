@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0..\build\SeiSeeMp"
C:\ProgramData\anaconda3\Library\bin\qmake.exe ..\..\SeiSeeMp\SeiSeeMp.pro -o Makefile.fk "CONFIG+=release fk_regression" "CONFIG-=debug" "QMAKE_CXXFLAGS+=/utf-8" "QMAKE_CXXFLAGS+=/FS" "OBJECTS_DIR=fk-obj" "MOC_DIR=fk-moc" "RCC_DIR=fk-rcc"
if errorlevel 1 exit /b 1
nmake /NOLOGO /F Makefile.fk > fk-build.log 2>&1
if errorlevel 1 exit /b 1
set "PATH=C:\ProgramData\anaconda3\Library\bin;%PATH%"
set "QT_QPA_PLATFORM=offscreen"
set "QT_PLUGIN_PATH=C:\ProgramData\anaconda3\Library\plugins"
if not defined SEISEE_TEST_OUT set "SEISEE_TEST_OUT=%~dp0..\build\fk-qa"
if not exist "%SEISEE_TEST_OUT%" mkdir "%SEISEE_TEST_OUT%"
release\fk_regression.exe -o "%SEISEE_TEST_OUT%\results.txt",txt
exit /b %errorlevel%
