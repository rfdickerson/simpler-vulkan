@echo off
echo Compiling shaders...

glslc text.vert -o text.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile text.vert
    exit /b 1
)

glslc text.frag -o text.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile text.frag
    exit /b 1
)

echo All shaders compiled successfully!

