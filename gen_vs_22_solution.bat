@echo off
REM Navigate to the project root directory (if not already there)
cd %~dp0

REM Create a directory for the build files if it doesn't exist
if not exist "build" (
    mkdir build
)

REM Navigate to the build directory
cd build

REM Run CMake to generate the Visual Studio 2022 solution
cmake -G "Visual Studio 17 2022" ..

REM Navigate back to the root directory
cd ..

echo Visual Studio 2022 solution generated successfully.
pause
