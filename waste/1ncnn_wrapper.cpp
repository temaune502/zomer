#include <windows.h>
#include <stdio.h>
#include <algorithm>
#include "ncnn_wrapper.h"

// Визначаємо типи для C-API функцій NCNN
typedef void* (*D_ncnn_net_create)();
typedef void (*D_ncnn_net_destroy)(void*);
typedef int (*D_ncnn_net_load_param)(void*, const char*);
typedef int (*D_ncnn_net_load_model)(void*, const char*);
typedef void (*D_ncnn_net_set_vulkan_device)(void*, int);

typedef void* (*D_ncnn_mat_create_3d)(int, int, int, size_t);
typedef void (*D_ncnn_mat_destroy)(void*);
typedef void* (*D_ncnn_mat_from_pixels)(const unsigned char*, int, int, int, int);
typedef void (*D_ncnn_mat_to_pixels)(const void*, unsigned char*, int, int);

typedef void* (*D_ncnn_extractor_create)(void*);
typedef void (*D_ncnn_extractor_destroy)(void*);
typedef int (*D_ncnn_extractor_input)(void*, const char*, const void*);
typedef int (*D_ncnn_extractor_extract)(void*, const char*, void*);

// Покажчики на функції (Тепер тут все на місці!)
D_ncnn_net_create p_ncnn_net_create = NULL;
D_ncnn_net_destroy p_ncnn_net_destroy = NULL;
D_ncnn_net_load_param p_ncnn_net_load_param = NULL;
D_ncnn_net_load_model p_ncnn_net_load_model = NULL;
D_ncnn_net_set_vulkan_device p_ncnn_net_set_vulkan_device = NULL;

D_ncnn_mat_create_3d p_ncnn_mat_create_3d = NULL; // <-- Виправлено тут
D_ncnn_mat_from_pixels p_ncnn_mat_from_pixels = NULL;
D_ncnn_mat_to_pixels p_ncnn_mat_to_pixels = NULL;
D_ncnn_mat_destroy p_ncnn_mat_destroy = NULL;

D_ncnn_extractor_create p_ncnn_extractor_create = NULL;
D_ncnn_extractor_destroy p_ncnn_extractor_destroy = NULL;
D_ncnn_extractor_input p_ncnn_extractor_input = NULL;
D_ncnn_extractor_extract p_ncnn_extractor_extract = NULL;

void* global_net = NULL; 
int current_gpu = -1;

extern "C" int init_neural_network(const char* param_path, const char* bin_path) {
    HMODULE hNcnn = LoadLibraryA("ncnn.dll");
    if (!hNcnn) {
        printf("[NCNN] Error: Cannot load ncnn.dll!\n");
        return 0; 
    }

    // Лінк підсистем мережі
    p_ncnn_net_create = (D_ncnn_net_create)GetProcAddress(hNcnn, "ncnn_net_create");
    p_ncnn_net_destroy = (D_ncnn_net_destroy)GetProcAddress(hNcnn, "ncnn_net_destroy");
    p_ncnn_net_load_param = (D_ncnn_net_load_param)GetProcAddress(hNcnn, "ncnn_net_load_param");
    p_ncnn_net_load_model = (D_ncnn_net_load_model)GetProcAddress(hNcnn, "ncnn_net_load_model");
    p_ncnn_net_set_vulkan_device = (D_ncnn_net_set_vulkan_device)GetProcAddress(hNcnn, "ncnn_net_set_vulkan_device");

    // Лінк роботи з матрицями пікселів
    p_ncnn_mat_create_3d = (D_ncnn_mat_create_3d)GetProcAddress(hNcnn, "ncnn_mat_create_3d"); // <-- Виправлено тут
    p_ncnn_mat_from_pixels = (D_ncnn_mat_from_pixels)GetProcAddress(hNcnn, "ncnn_mat_from_pixels");
    p_ncnn_mat_to_pixels = (D_ncnn_mat_to_pixels)GetProcAddress(hNcnn, "ncnn_mat_to_pixels");
    p_ncnn_mat_destroy = (D_ncnn_mat_destroy)GetProcAddress(hNcnn, "ncnn_mat_destroy");

    // Лінк екстрактора виконання
    p_ncnn_extractor_create = (D_ncnn_extractor_create)GetProcAddress(hNcnn, "ncnn_extractor_create");
    p_ncnn_extractor_destroy = (D_ncnn_extractor_destroy)GetProcAddress(hNcnn, "ncnn_extractor_destroy");
    p_ncnn_extractor_input = (D_ncnn_extractor_input)GetProcAddress(hNcnn, "ncnn_extractor_input");
    p_ncnn_extractor_extract = (D_ncnn_extractor_extract)GetProcAddress(hNcnn, "ncnn_extractor_extract");

    // Валідація лінкінгу
    if (!p_ncnn_net_create || !p_ncnn_net_load_param || !p_ncnn_net_load_model || 
        !p_ncnn_mat_from_pixels || !p_ncnn_mat_to_pixels || !p_ncnn_mat_destroy || 
        !p_ncnn_mat_create_3d || !p_ncnn_extractor_create) {
        printf("[NCNN] Error: Failed to map core dynamic symbols!\n");
        return 0;
    }

    global_net = p_ncnn_net_create();
    if (!global_net) return 0;

    if (p_ncnn_net_load_param(global_net, param_path) != 0 || p_ncnn_net_load_model(global_net, bin_path) != 0) {
        printf("[NCNN] Error: Failed to load network layers or weights.\n");
        return 0;
    }
    
    printf("[NCNN] Pipeline successfully initialized and ready.\n");
    return 1; 
}

extern "C" void run_neural_upscale(const unsigned char* src, int w, int h, unsigned char* dst, int dw, int dh, int tile_size, int tile_padding, int gpu_device) {
    if (p_ncnn_net_set_vulkan_device && current_gpu != gpu_device) {
        current_gpu = gpu_device;
        p_ncnn_net_set_vulkan_device(global_net, current_gpu);
    }

    int scale = dw / w; 
    int tiles_x = (w + tile_size - 1) / tile_size;
    int tiles_y = (h + tile_size - 1) / tile_size;

    int max_tile_w = tile_size + tile_padding * 2;
    int max_tile_h = tile_size + tile_padding * 2;
    unsigned char* tile_src_rgba = (unsigned char*)malloc(max_tile_w * max_tile_h * 4);
    unsigned char* tile_dst_rgba = (unsigned char*)malloc(max_tile_w * scale * max_tile_h * scale * 4);

    for (int yi = 0; yi < tiles_y; yi++) {
        for (int xi = 0; xi < tiles_x; xi++) {
            
            int src_x0 = xi * tile_size;
            int src_y0 = yi * tile_size;
            int src_x1 = std::min(src_x0 + tile_size, w);
            int src_y1 = std::min(src_y0 + tile_size, h);

            int tile_src_x0 = std::max(src_x0 - tile_padding, 0);
            int tile_src_y0 = std::max(src_y0 - tile_padding, 0);
            int tile_src_x1 = std::min(src_x1 + tile_padding, w);
            int tile_src_y1 = std::min(src_y1 + tile_padding, h);

            int current_tile_w = tile_src_x1 - tile_src_x0;
            int current_tile_h = tile_src_y1 - tile_src_y0;

            for (int y = 0; y < current_tile_h; y++) {
                const unsigned char* src_ptr = src + ((tile_src_y0 + y) * w + tile_src_x0) * 4;
                unsigned char* dst_ptr = tile_src_rgba + (y * current_tile_w) * 4;
                memcpy(dst_ptr, src_ptr, current_tile_w * 4);
            }

            void* in_mat = p_ncnn_mat_from_pixels(tile_src_rgba, 4, current_tile_w, current_tile_h, current_tile_w * 4);

            void* ex = p_ncnn_extractor_create(global_net);
            p_ncnn_extractor_input(ex, "in0", in_mat);

            void* out_mat = p_ncnn_mat_create_3d(0, 0, 0, sizeof(float)); 
            p_ncnn_extractor_extract(ex, "out0", out_mat);

            int out_tile_w = current_tile_w * scale;
            int out_tile_h = current_tile_h * scale;

            p_ncnn_mat_to_pixels(out_mat, tile_dst_rgba, 4, out_tile_w * 4);

            int actual_out_x0 = src_x0 * scale;
            int actual_out_y0 = src_y0 * scale;
            int actual_out_x1 = src_x1 * scale;
            int actual_out_y1 = src_y1 * scale;

            int dst_sub_w = actual_out_x1 - actual_out_x0;
            int dst_sub_h = actual_out_y1 - actual_out_y0;

            int pad_offset_x = (src_x0 - tile_src_x0) * scale;
            int pad_offset_y = (src_y0 - tile_src_y0) * scale;

            for (int y = 0; y < dst_sub_h; y++) {
                const unsigned char* tile_ptr = tile_dst_rgba + ((pad_offset_y + y) * out_tile_w + pad_offset_x) * 4;
                unsigned char* global_dst_ptr = dst + ((actual_out_y0 + y) * dw + actual_out_x0) * 4;
                memcpy(global_dst_ptr, tile_ptr, dst_sub_w * 4);
            }

            p_ncnn_extractor_destroy(ex);
            p_ncnn_mat_destroy(in_mat);
            p_ncnn_mat_destroy(out_mat);
        }
    }

    free(tile_src_rgba);
    free(tile_dst_rgba);
}

extern "C" void destroy_neural_network() {
    if (global_net && p_ncnn_net_destroy) {
        p_ncnn_net_destroy(global_net);
        global_net = NULL;
    }
    printf("[NCNN] Resources successfully released.\n");
}