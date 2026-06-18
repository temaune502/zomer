#gcc -c glfw\src\glad.c -o build\glad.o -Wall -Wextra -march=native -Iglfw\include\GLFW

gcc -c main_enchanced.c -o build\main_enchanced.o -march=native -Wall -Wextra -Iglfw\include\GLFW

gcc build\main_enchanced.o build\glad.o -o 2zomer.exe -march=native -s -Wall -Wextra ^
-Lglfw\lib-mingw-w64 ^
-lglfw3 ^
-lgdi32 ^
-static ^
-static-libgcc
