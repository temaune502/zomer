#ifndef NCNN_WRAPPER_H
#define NCNN_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

int init_neural_network(const char* param_path, const char* bin_path);
void run_neural_upscale(const unsigned char* src, int w, int h, unsigned char* dst, int dw, int dh, int tile_size, int tile_padding, int gpu_device);
void destroy_neural_network();

#ifdef __cplusplus
}
#endif

#endif // NCNN_WRAPPER_H