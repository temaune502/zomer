gcc -c libs\glfw\src\glad.c -o build\glad.o -Ilibs\glfw\include\GLFW

gcc -c main_enchanced_extend.c -o build\main_enchanced_extend.o -Ilibs\glfw\include\GLFW -Ilibs\onnxruntime_portabl\include

gcc -c ui.c -o build\ui.o -Ilibs\glfw\include\GLFW -Ilibs\onnxruntime_portabl\include

gcc -c upscale.c -o build\upscale.o  -Ilibs\glfw\include\GLFW

gcc build\main_enchanced_extend.o build\ui.o build\glad.o build\upscale.o -o extend_zomer.exe ^
-Llibs\glfw\lib-mingw-w64 ^
-Llibs\onnxruntime_portabl\libs ^
-lglfw3 ^
-lonnxruntime ^
-lgdi32 ^
-lole32 ^
-static ^
-static-libgcc
