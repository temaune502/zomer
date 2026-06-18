gcc test.c -o test.exe \
-static-libgcc \
-I./onnxruntime_portabl/include \
-Wall -Wextra \
-Le:/Probes/onnxruntime/build/Windows/Release \
"./onnxruntime_portabl/libs/onnxruntime.lib" \
-Le:/Probes/upscale \
-lonnxruntime 