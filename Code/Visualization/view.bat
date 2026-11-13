@echo off
REM Otwiera przegladarke klatek Lenia dla podanego folderu z plikami frame_*.pgm
REM Uzycie:  view.bat FOLDER [--fps N] [--zoom N]
REM          view.bat                 - bez argumentu otwiera folder "Results" w biezacym katalogu
REM Mozna tez przeciagnac folder z wynikami na ten plik w Eksploratorze.

python "%~dp0lenia_viewer.py" %*

REM przy bledzie nie zamykaj okna od razu, zeby dalo sie przeczytac komunikat
if errorlevel 1 pause
