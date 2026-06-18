gcc -c libs\glfw\src\glad.c -o build\glad.o -Wall -Wextra -march=native -Ilibs\glfw\include\GLFW -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 

gcc -c ui.c -o build\ui.o -Ilibs\glfw\include\GLFW -Ilibs\onnxruntime_portabl\include -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 


gcc -c main_enchanced_extend.c -o build\main_enchanced_extend.o -march=native -Wall -Wextra -Ilibs\glfw\include\GLFW -Ilibs\onnxruntime_portabl\include -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 

gcc -c upscale.c -o build\upscale.o -march=native -Wall -Wextra -Ilibs\glfw\include\GLFW -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 

gcc build\main_enchanced_extend.o build\ui.o build\glad.o build\upscale.o -o rzomer.exe -march=native -mwindows -s -Wall -Wextra -O3 -flto -ffast-math -funroll-loops -finline-functions --param inline-unit-growth=200 --param max-inline-insns-single=500 ^
-Llibs\glfw\lib-mingw-w64 ^
-Llibs\onnxruntime_portabl\libs ^
-lglfw3 ^
-lonnxruntime ^
-lgdi32 ^
-lole32 ^
-static ^
-static-libgcc
