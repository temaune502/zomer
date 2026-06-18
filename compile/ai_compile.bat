::#gcc -c glfw\src\glad.c -o build\glad.o -Wall -Wextra -march=native -Iglfw\include\GLFW
::#g++ -c ncnn_wrapper.cpp -o build\ncnn_wrapper.o -O3

g++ -c ncnn_wrapper.cpp -o build\ncnn_wrapper.o -O3 -std=c++11 -I"ncnn/include" 


gcc -c ai.c -o build\ai.o -O3 -Iglfw\include\GLFW


	

g++ build\ai.o build\ncnn_wrapper.o build\glad.o -o ai.exe ^
    -Lglfw\lib-mingw-w64 -lglfw3 ^
    "C:\VulkanSDK\1.4.350.0\Lib\vulkan-1.lib" ^
    -lgdi32 -luser32 -lkernel32 -lcomdlg32 -lole32 -lshell32