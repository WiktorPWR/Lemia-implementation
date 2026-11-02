@echo off
REM Kompilacja (Makefile + g++) i uruchomienie projektu Only_CPU
REM Uzycie:  build_and_run.bat           - release, buduje i uruchamia
REM          build_and_run.bat debug     - debug, buduje i uruchamia
REM          build_and_run.bat build     - release, tylko buduje
REM          build_and_run.bat clean     - usuwa folder build
setlocal
cd /d "%~dp0"

where mingw32-make >nul 2>nul || (
  echo Nie znaleziono mingw32-make - dodaj folder bin MinGW do PATH.
  exit /b 1
)

set MODE=release
set GOAL=run
if /i "%~1"=="debug" set MODE=debug
if /i "%~1"=="build" set GOAL=all
if /i "%~1"=="clean" set GOAL=clean

mingw32-make -j%NUMBER_OF_PROCESSORS% MODE=%MODE% %GOAL%
