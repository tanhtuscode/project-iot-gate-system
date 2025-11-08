@echo off
echo ========================================
echo ESP32 Upload Script
echo ========================================
echo.
echo Detecting available COM ports...
echo.

arduino-cli-install\arduino-cli.exe board list

echo.
echo ========================================
echo Enter your ESP32's COM port (e.g., COM3, COM5, etc.):
set /p COMPORT=

echo.
echo Uploading to %COMPORT%...
echo.

arduino-cli-install\arduino-cli.exe upload --fqbn esp32:esp32:esp32 -p %COMPORT% --input-dir "C:\Users\natha\AppData\Local\Temp\arduino\sketches\4AE68C80169C801DBEC839330096B3DF" .

echo.
echo ========================================
echo Upload complete!
echo.
echo Now open Serial Monitor to see output:
echo arduino-cli-install\arduino-cli.exe monitor -p %COMPORT% -c baudrate=9600
echo ========================================
pause
