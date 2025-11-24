@echo off
echo Building Portal Parabox Game...

:: Create build directory
if not exist build mkdir build
cd build

:: Compile the game
g++ -std=c++17 ^
    -I../include ^
    ../src/level_loader.cpp ^
    ../src/gameplay.cpp ^
    ../test/interface.cpp ^
    ../test/main.cpp ^
    -o portal_parabox.exe

if %errorlevel% equ 0 (
    echo Build successful!
    echo Run with: cd build && portal_parabox.exe
) else (
    echo Build failed!
)

cd ..
