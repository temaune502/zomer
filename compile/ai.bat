gcc -c main_enchanced_extend.c -o build\main_enchanced_extend.o  -Ilibs\onnxruntime_portabl\include -Ilibs\glfw\include\GLFW

gcc -c upscale.c -o build\upscale.o  -Ilibs\glfw\include\GLFW

gcc build\main_enchanced_extend.o build\glad.o build\upscale.o -o extend_zomer.exe ^
    -Llibs\glfw\lib-mingw-w64 ^
    "libs/onnxruntime_portabl/libs/onnxruntime.lib" ^
    -lglfw3 ^
    -lgdi32 ^
    -static ^
    -static-libgcc
