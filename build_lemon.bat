@echo off
echo ========================================
echo Building Hi Lemon with Edge Impulse
echo ========================================
call C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat
idf.py build
