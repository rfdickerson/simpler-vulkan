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

glslc triangle.vert -o triangle.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile triangle.vert
    exit /b 1
)

glslc triangle.frag -o triangle.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile triangle.frag
    exit /b 1
)

glslc terrain.vert -o terrain.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile terrain.vert
    exit /b 1
)

glslc terrain.frag -o terrain.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile terrain.frag
    exit /b 1
)

glslc terrain_cull.comp -o terrain_cull.comp.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile terrain_cull.comp
    exit /b 1
)

glslc tree.vert -o tree.vert.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile tree.vert
    exit /b 1
)

glslc tree.frag -o tree.frag.spv
if %ERRORLEVEL% NEQ 0 (
    echo Failed to compile tree.frag
    exit /b 1
)

echo All shaders compiled successfully!

