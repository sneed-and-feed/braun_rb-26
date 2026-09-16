@echo off
setlocal enabledelayedexpansion
title Deploy to GitHub Pages (sneed-and-feed.github.io)

cd /d "%~dp0"

echo ====================================================================
echo   BRAUN RB-26 - DEPLOY TO GITHUB PAGES (sneed-and-feed.github.io)
echo ====================================================================
echo.

set "TARGET_DIR=%~dp0..\sneed-and-feed.github.io"
set "RB26_DEST=%TARGET_DIR%\rb-26"

:: Check if sneed-and-feed.github.io clone exists
if not exist "%TARGET_DIR%\.git" (
    echo [INFO] Local clone not found at "%TARGET_DIR%".
    echo Cloning https://github.com/sneed-and-feed/sneed-and-feed.github.io.git ...
    git clone https://github.com/sneed-and-feed/sneed-and-feed.github.io.git "%TARGET_DIR%"
    if !errorlevel! neq 0 (
        echo [ERROR] Failed to clone repository.
        pause
        exit /b 1
    )
)

echo [1/4] Pulling latest changes in sneed-and-feed.github.io ...
cd /d "%TARGET_DIR%"
git pull origin main

echo [2/4] Syncing assets from braun_rb-26 ...
cd /d "%~dp0"

if not exist "%RB26_DEST%" mkdir "%RB26_DEST%"
if not exist "%TARGET_DIR%\releases" mkdir "%TARGET_DIR%\releases"

copy /y "web\index.html" "%RB26_DEST%\" >nul
copy /y "web\.nojekyll" "%RB26_DEST%\" >nul 2>nul
copy /y ".nojekyll" "%RB26_DEST%\" >nul 2>nul

robocopy "web\css" "%RB26_DEST%\css" /E /NFL /NDL /NJH /NJS >nul
robocopy "web\js" "%RB26_DEST%\js" /E /NFL /NDL /NJH /NJS >nul
robocopy "web\presets" "%RB26_DEST%\presets" /E /NFL /NDL /NJH /NJS >nul

if exist "releases" (
    copy /y "releases\BRAUN_RB26-*.zip" "%TARGET_DIR%\releases\" >nul 2>nul
)

echo [3/4] Checking git status ...
cd /d "%TARGET_DIR%"
git status --short

git add rb-26 releases -A
git diff --cached --quiet
if !errorlevel! equ 0 (
    echo [INFO] No changes to deploy - sneed-and-feed.github.io is already up-to-date!
    goto done
)

echo [4/4] Committing and pushing to GitHub Pages ...
for /f "tokens=2 delims==" %%I in ('wmic os get localdatetime /value') do set datetime=%%I
set timestamp=!datetime:~0,4!-!datetime:~4,2!-!datetime:~6,2! !datetime:~8,2!:!datetime:~10,2!
git commit -m "deploy: update Braun RB-26 reverberator build (%timestamp%)"
git push origin main

if !errorlevel! equ 0 (
    echo.
    echo ====================================================================
    echo   DEPLOY SUCCESSFUL!
    echo   Reverberator is live at: https://sneed-and-feed.github.io/rb-26/
    echo ====================================================================
) else (
    echo.
    echo [ERROR] git push failed. Please verify git permissions or network connection.
)

:done
echo.
pause
endlocal
