#define STB_IMAGE_IMPLEMENTATION
#include "glfw\stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "glfw\stb_image_write.h"

#include "c_api.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("[*] Starting NCNN C-API Test with STB_Image...\n");

    // 1. Завантажуємо вхідне зображення (примусово декодуємо в 4 канали - RGBA)
    int width, height, channels;
    unsigned char* raw_pixels = stbi_load("input.png", &width, &height, &channels, 4);
    if (!raw_pixels) {
        fprintf(stderr, "[ERROR] Could not open or find input.png\n");
        return -1;
    }
    printf("[*] Loaded input.png: %dx%d, channels: %d (forced to 4 for RGBA)\n", width, height, channels);

    // Припустимо, що наша модель робить апскейл х2 (як Real-ESRGAN/Nomos)
    // Якщо твоя модель не змінює роздільну здатність, постав тут такі ж width/height
    int out_width = width * 2;
    int out_height = height * 2;

    // 2. Створення опцій та увімкнення Вулкана
    ncnn_option_t opt = ncnn_option_create();
    ncnn_option_set_use_vulkan_compute(opt, 1);
    
    // 3. Створення мережі
    ncnn_net_t net = ncnn_net_create();
    ncnn_net_set_option(net, opt);
    
    // Завантаження бінарників моделі
    int res_param = ncnn_net_load_param(net, "model.param");
    int res_model = ncnn_net_load_model(net, "model.bin");
    printf("[*] Model load status: param=%d, bin=%d\n", res_param, res_model);
    
    // 4. Створення вхідної матриці з завантажених пікселів (крок/stride = width * 4)
    ncnn_mat_t input_mat = ncnn_mat_from_pixels(raw_pixels, NCNN_MAT_PIXEL_RGBA, width, height, width * 4, 0);
    
    // 5. Екстрактор для запуску нейромережі
    ncnn_extractor_t ex = ncnn_extractor_create(net);
    ncnn_extractor_input(ex, "in0", input_mat); // за потреби зміни ім'я входу під свою модель
    
    // 6. Отримуємо результат
    ncnn_mat_t output_mat = ncnn_mat_create();
    ncnn_extractor_extract(ex, "out0", &output_mat); // за потреби зміни ім'я виходу під свою модель
    
    // 7. Виділяємо пам'ять під вихідні дані та вивантажуємо пікселі
    unsigned char* out_pixels = (unsigned char*)malloc(out_width * out_height * 4);
    if (out_pixels) {
        ncnn_mat_to_pixels(output_mat, out_pixels, NCNN_MAT_PIXEL_RGBA, out_width * 4);
        
        // Зберігаємо оброблену картинку назад на диск через stb_image_write
        if (stbi_write_png("output.png", out_width, out_height, 4, out_pixels, out_width * 4)) {
            printf("[SUCCESS] Processed image saved to output.png (%dx%d)\n", out_width, out_height);
        } else {
            fprintf(stderr, "[ERROR] Failed to write output.png\n");
        }
        free(out_pixels);
    }
    
    // 8. Чистимо абсолютно всі ресурси
    ncnn_mat_destroy(input_mat);
    ncnn_mat_destroy(output_mat);
    ncnn_extractor_destroy(ex);
    ncnn_net_destroy(net);
    ncnn_option_destroy(opt);
    stbi_image_free(raw_pixels);

    return 0;
}