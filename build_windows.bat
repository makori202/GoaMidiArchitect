@echo off
echo Configuring GoaMidiArchitect VST3 (JUCE 8)...
cmake -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto err
echo Building Release VST3...
cmake --build build --config Release --target GoaMidiArchitect_VST3 -j 4
if errorlevel 1 goto err
echo.
echo SUCCESS! Copy the VST3 folder below to C:\Program Files\Common Files\VST3\
dir /b "build\GoaMidiArchitect_artefacts\Release\VST3"
pause
exit /b 0
:err
echo Build failed. Make sure Visual Studio 2022 (C++ Desktop) and CMake are installed.
pause
