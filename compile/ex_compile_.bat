#gcc -c glfw\src\glad.c -o build\glad.o -Wall -Wextra -march=native -Iglfw\include\GLFW

gcc -c main_enchanced_extend.c -o build\main_enchanced_extend.o -march=native -Wall -Wextra -Iglfw\include\GLFW

gcc -c upscale.c -o build\upscale.o -march=native -Wall -Wextra -Iglfw\include\GLFW

gcc build\main_enchanced_extend.o build\glad.o build\upscale.o -o extend_zomer.exe -march=native -s -Wall -Wextra ^
-Lglfw\lib-mingw-w64 ^
-lglfw3 ^
-lgdi32 ^
-static ^
-static-libgcc
