@echo off
REM Budowanie (CMake + Ninja + nvcc) i uruchomienie projektu CPU_and_GPU
REM Uzycie:  build_and_run.bat           - Release, buduje i uruchamia
REM          build_and_run.bat debug     - Debug, buduje i uruchamia
REM          build_and_run.bat build     - Release, tylko buduje
REM          build_and_run.bat clean     - usuwa folder build
REM          build_and_run.bat show      - Release: buduje, czysci Results, uruchamia, robi GIF, otwiera okno
REM          build_and_run.bat view      - samo okno z wynikami (Results), bez liczenia od nowa
REM          build_and_run.bat gif       - GIF z istniejacych klatek (Results\animation.gif)
setlocal
cd /d "%~dp0"

set CONFIG=Release
set RUN=1
if /i "%~1"=="debug" set CONFIG=Debug
if /i "%~1"=="build" set RUN=0
if /i "%~1"=="clean" (
  if exist build rmdir /s /q build
  if exist build (
    echo Nie udalo sie usunac build - zamknij program lub terminal, ktory go uzywa.
    exit /b 1
  )
  echo Usunieto folder build.
  exit /b 0
)

REM Wizualizacja (Python) - wspolna dla wszystkich wersji, w ..\Visualization.
REM "Results" musi byc tym samym folderem co OUTPUT_FOLDER w main.cpp.
set "VIS=%~dp0..\Visualization"
if /i "%~1"=="view" ( python "%VIS%\lenia_viewer.py" Results & exit /b )
if /i "%~1"=="gif"  ( python "%VIS%\make_gif.py" Results & exit /b )
set SHOW=0
if /i "%~1"=="show" set SHOW=1

REM nvcc potrzebuje cl.exe, a CMake i Ninja sa w Visual Studio Build Tools.
REM Jesli nie ma ich w PATH - ladujemy srodowisko VS (vcvars64.bat).
where cl >nul 2>nul && where cmake >nul 2>nul && where ninja >nul 2>nul && goto :env_ok
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
  echo Nie znaleziono Visual Studio z kompilatorem C++ i CMake.
  exit /b 1
)
REM vcvars64 sam szuka vswhere.exe w PATH - dodajemy go, zeby nie bylo komunikatu o bledzie
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
REM vcvars potrafi zmienic katalog roboczy - wracamy do folderu skryptu
cd /d "%~dp0"
:env_ok

cmake -S . -B "build\%CONFIG%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 ( echo. & echo Blad konfiguracji CMake. & exit /b 1 )
REM kopia dla clangd - szuka compile_commands.json w folderze projektu
copy /y "build\%CONFIG%\compile_commands.json" compile_commands.json >nul

cmake --build "build\%CONFIG%"
if errorlevel 1 ( echo. & echo Blad kompilacji. & exit /b 1 )

if "%RUN%"=="0" exit /b 0
REM show: stare klatki usuwamy PRZED symulacja, zeby nie mieszaly sie z nowymi
if "%SHOW%"=="1" if exist Results rmdir /s /q Results
echo.
"build\%CONFIG%\CPU_and_GPU.exe"
REM blad programu - nie robimy GIF-a ani okna (wymarcie zycia to NIE blad, kod 0)
if errorlevel 1 exit /b 1
if "%SHOW%"=="0" exit /b 0
python "%VIS%\make_gif.py" Results
python "%VIS%\lenia_viewer.py" Results
