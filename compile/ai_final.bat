
gcc -c libs\glfw\src\glad.c -o build\glad.o -Wall -Wextra -march=native -Ilibs\glfw\include\GLFW -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 

gcc -c main_enchanced_extend.c -o build\main_enchanced_extend.o -Ilibs\onnxruntime_portabl\include -Ilibs\glfw\include\GLFW -march=native -mwindows -s -Wall -Wextra -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500
gcc -c upscale.c -o build\upscale.o  -Ilibs\glfw\include\GLFW -march=native -mwindows -s -Wall -Wextra -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500

gcc build\main_enchanced_extend.o build\glad.o build\upscale.o -o rzomer.exe ^
    -march=native -mwindows -s -Wall -Wextra -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 ^
    -Llibs\glfw\lib-mingw-w64 ^
    "libs/onnxruntime_portabl/libs/onnxruntime.lib" ^
    -lglfw3 ^
    -lgdi32 ^
    -static ^
    -static-libgcc
