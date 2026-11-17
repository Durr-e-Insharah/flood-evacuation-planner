@echo off
REM Compile and run the GUI version (needs SFML 3: pacman -S mingw-w64-ucrt-x86_64-sfml)
g++ -std=c++17 -O2 gui/gui_main.cpp src/graph.cpp src/planner.cpp -o planner_gui.exe -lsfml-graphics -lsfml-window -lsfml-system
if errorlevel 1 (
    echo Build failed. Is SFML installed? See README.md
    exit /b 1
)
echo Build OK. Starting GUI...
planner_gui.exe data/map.txt
