@echo off
setlocal enabledelayedexpansion
REM ==========================================================================
REM  GrainFreeze - push to GitHub (one double-click).
REM  Put this file inside the extracted GrainFreeze folder and double-click it.
REM  It will ask for your GitHub username, then push. When Git asks you to log
REM  in (a browser window opens), that is your account - complete it once.
REM ==========================================================================

cd /d "%~dp0"
echo.
echo  ==== GrainFreeze -> GitHub ====
echo  Cartella: %cd%
echo.

REM --- check git ---
where git >nul 2>nul
if errorlevel 1 (
    echo  [ERRORE] Git non e' installato.
    echo  Scaricalo da https://git-scm.com/download/win , installalo e riprova.
    echo.
    pause
    exit /b 1
)

REM --- ask for username ---
set /p GHUSER=  Inserisci il tuo nome utente GitHub e premi Invio:
if "%GHUSER%"=="" (
    echo  [ERRORE] Nome utente vuoto. Riprova.
    pause
    exit /b 1
)

set REPO=https://github.com/%GHUSER%/grainfreeze.git
echo.
echo  Repository: %REPO%
echo.

REM --- init if needed ---
if not exist ".git" (
    echo  Inizializzo il repository locale...
    git init
)

REM --- identity (only sets if missing; harmless if already set) ---
git config user.name  >nul 2>nul || git config user.name  "%GHUSER%"
git config user.email >nul 2>nul || git config user.email "%GHUSER%@users.noreply.github.com"

echo  Aggiungo i file...
git add .

echo  Creo il commit...
git commit -m "Initial commit: GrainFreeze VST3 scaffold" -m "Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>" 2>nul
if errorlevel 1 (
    echo  (Nessuna modifica nuova da committare, proseguo.)
)

echo  Imposto il branch main...
git branch -M main

REM --- set or update the remote ---
git remote get-url origin >nul 2>nul
if errorlevel 1 (
    git remote add origin %REPO%
) else (
    git remote set-url origin %REPO%
)

echo.
echo  Eseguo il push. Se si apre il browser, accedi a GitHub una volta.
echo.
git push -u origin main

echo.
if errorlevel 1 (
    echo  [ATTENZIONE] Il push non e' andato a buon fine.
    echo  Cause comuni:
    echo   - login GitHub annullato: rilancia lo script.
    echo   - la repo online non e' vuota: scrivimi l'errore e ti dico come fare.
) else (
    echo  [OK] Fatto! Apri la tab "Actions" della repo su GitHub:
    echo       la build parte da sola. A fine build scarichi il VST3 dagli Artifacts.
)
echo.
pause
endlocal
