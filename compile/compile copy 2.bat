gcc -c glfw\src\glad.c -o build\glad.o ^
-O3 -flto -march=native -ffast-math -funroll-loops -finline-functions ^
--param inline-unit-growth=200 --param max-inline-insns-single=500 ^
-Iglfw\include\GLFW
gcc -c main_enchanced.c -o build\main_enchanced.o ^
-O3 -flto -march=native -ffast-math -funroll-loops -finline-functions ^
--param inline-unit-growth=200 --param max-inline-insns-single=500 ^
-Iglfw\include\GLFW

gcc build\main_enchanced.o build\glad.o -o 3zomer.exe ^
-O3 -flto -march=native -ffast-math -mwindows -s ^
-Lglfw\lib-mingw-w64 ^
-lglfw3 ^
-lgdi32 ^
-static ^
-static-libgcc
