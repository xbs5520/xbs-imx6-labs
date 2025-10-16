@echo off
echo ========================================
echo Performance Analyzer - IMX6ULL Project
echo ========================================
echo.
echo Collecting data for 30 seconds...
echo.
python performance_analyzer.py COM5 30
echo.
echo Analysis complete!
pause
