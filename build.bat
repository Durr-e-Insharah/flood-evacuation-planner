@echo off
REM Compile and run (Windows, MSYS2 g++ must be on PATH)
g++ -std=c++17 -Wall -Wextra -O2 src/*.cpp -o planner.exe
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)
echo Build OK. Starting planner...
planner.exe data/map.txt
