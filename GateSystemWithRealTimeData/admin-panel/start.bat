@echo off
echo ========================================
echo   RFID Gate System - Admin Dashboard
echo ========================================
echo.
echo Starting the admin panel server...
echo.
echo Once started, open your browser and go to:
echo http://localhost:3000
echo.
echo Press Ctrl+C to stop the server
echo.
echo ========================================
echo.

cd /d "%~dp0"
npm start
