cc -c glfw\src\glad.c ^
    -o glad.o -flto -ffast-math -funroll-loops -finline-functions  ^
    -O3 ^
    -Iglfw\include\GLFW

cc -c main.c ^
    -o main.o  -flto -ffast-math -funroll-loops -finline-functions  ^
    -O3 ^
    -Iglfw\include\GLFW

cc main.o glad.o ^
    -o zomer.exe ^
    -mwindows -flto -ffast-math -funroll-loops -finline-functions  ^
    -Lglfw\lib-mingw-w64 ^
    -lglfw3 ^
    -lgdi32 ^
    -static ^
    -static-libgcc