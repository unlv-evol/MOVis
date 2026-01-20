@echo off
set EXE=%1
set DEPLOY_DIR=%2
mkdir "%DEPLOY_DIR%"
windeployqt "%EXE%" --dir "%DEPLOY_DIR%"

