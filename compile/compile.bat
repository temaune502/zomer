gcc -c glfw\src\glad.c -o build\glad.o -Wall -Wextra ^
-O3 -march=native -flto -ffast-math -funroll-loops -finline-functions ^
--param inline-unit-growth=200 --param max-inline-insns-single=500 ^
-Iglfw\include\GLFW
gcc -c main.c -o build\main.o ^
-O3 -flto -march=native -ffast-math -funroll-loops -finline-functions -Wall -Wextra ^
--param inline-unit-growth=200 --param max-inline-insns-single=500 ^
-Iglfw\include\GLFW

gcc build\main.o build\glad.o -o zomer.exe ^
-O3 -flto -march=native -ffast-math -mwindows -s -Wall -Wextra ^
-Lglfw\lib-mingw-w64 ^
-lglfw3 ^
-lgdi32 ^
-static -static-libgcc
