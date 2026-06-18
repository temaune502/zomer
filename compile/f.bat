@echo off
cls
echo ===================================================
echo   ULTRA OPTIMIZED BUILD SYSTEM - ZOMER
echo ===================================================
echo.

:: 1. ПЕРЕВІРКА ТА КОМПІЛЯЦІЯ GLAD.C (Робиться 1 раз)
if not exist glad.o (
    echo [1/3] Compiling glad.c with maximum optimization...
    gcc -c glfw\src\glad.c -o glad.o ^
        -O3 -flto -march=native -ffast-math -funroll-loops -finline-functions ^
        --param inline-unit-growth=200 --param max-inline-insns-single=500 ^
        -Iglfw\include\GLFW
    if errorlevel 1 goto error
) else (
    echo [1/3] glad.o already exists. Skipping compilation.
)

:: 2. КОМПІЛЯЦІЯ MAIN.C (Щоразу при зміні коду)
echo [2/3] Compiling main.c with native CPU instructions...
gcc -c main.c -o main.o ^
    -O3 -flto -march=native -ffast-math -funroll-loops -finline-functions ^
    --param inline-unit-growth=200 --param max-inline-insns-single=500 ^
    -Iglfw\include\GLFW
if errorlevel 1 goto error

:: 3. ФІНАЛЬНЕ ЛІНКУВАННЯ (Збірка в EXE)
echo [3/3] Linking and performing Link-Time Optimization (LTO)...
gcc main.o glad.o -o zomer.exe ^
    -O3 -flto -march=native -ffast-math -mwindows -s^
    -Lglfw\lib-mingw-w64 ^
    -lglfw3 ^
    -lgdi32 ^
    -static ^
    -static-libgcc
if errorlevel 1 goto error

echo.
echo ===================================================
echo   SUCCESS! zomer.exe compiled at absolute maximum!
echo ===================================================
goto end

:error
echo.
echo ---------------------------------------------------
echo   [ERROR] Compilation failed. Check your code/paths.
echo ---------------------------------------------------

:end
pause