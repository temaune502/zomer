g++ AI_TEST.c c_api.cpp ^
"C:\Users\temaune\Desktop\SPAN-ncnn-vulkan\src\ncnn\src\gpu.cpp" ^
"C:\Users\temaune\Desktop\SPAN-ncnn-vulkan\src\ncnn\src\vulkandevice.cpp" ^
"C:\Users\temaune\Desktop\SPAN-ncnn-vulkan\src\ncnn\src\mat_pixel.cpp" ^
-I"C:\VulkanSDK\1.4.350.0\Include" ^
-I"C:\VulkanSDK\1.4.350.0\Include\glslang" ^
-I"./ncnn/include/ncnn" ^
-I"E:\Probes\zomer\ncnn\src" ^
-L"E:\Probes\zomer\ncnn\lib" ^
-L"C:\VulkanSDK\1.4.350.0\Lib" ^
-lncnn -lvulkan-1 -fopenmp -lgdi32 -lwinmm ^
-o test_ncnn.exe