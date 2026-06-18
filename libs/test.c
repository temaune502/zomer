#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Підключаємо stb для читання зображень
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Підключаємо stb для запису зображень
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Підключаємо нашу бібліотеку
#define AI_UPSCALER_IMPLEMENTATION
#include "ai_upscaler.h"

int main(int argc, char *argv[]) {

    (void)argc;

    const char* input_path = "input.png";
    const char* output_path = "output.png";
    const char* model_path =  argv[1]; //"models/4xNomos8k_span_otf_strong_fp16_opset16.onnx"; // Заміни на ім'я своєї моделі

    printf("--- AI Upscaler Test ---\n");

    // 1. Завантаження вхідного зображення
    int in_w, in_h, in_channels;
    // Примусово просимо 3 канали (RGB), оскільки наша модель налаштована на них
    unsigned char* input_pixels = stbi_load(input_path, &in_w, &in_h, &in_channels, 3);
    
    if (!input_pixels) {
        printf("[Error] Image dont load: %s\n", input_path);
        return 1;
    }
    printf("Loading image: %dx%d (Chanels: 3)\n", in_w, in_h);

    // 2. Налаштування апскейлера
    UpscalerConfig config = {
        .tile_size = 2048/2,       // Розмір тайлу (256 або 512 оптимально)
        .scale_factor = 4,      // Множник збільшення (має відповідати моделі!)
        .channels = 3,          //RGB
        .ep = UPSCALER_EP_DIRECTML   // Зміни на UPSCALER_EP_DIRECTML для апаратного прискорення на AMD/Intel
    };

    printf("Initialization ONNX Runtime...\n");
    clock_t start_time = clock();

    // 3. Створення контексту
    UpscalerContext* ctx = ai_upscaler_create(model_path, &config);
    if (!ctx) {
        printf("[Error] Failed to initialize upscaler. Check model path.\n");
        stbi_image_free(input_pixels);
        return 1;
    }

    printf("Image processing (this may take some time)...\n");

    // 4. Запуск обробки
    int out_w = 0, out_h = 0;
    unsigned char* output_pixels = ai_upscaler_process(ctx, input_pixels, in_w, in_h, &out_w, &out_h);

    clock_t end_time = clock();
    double time_spent = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // 5. Збереження результату та очищення пам'яті
    if (output_pixels) {
        printf("Success! New size: %dx%d. Processing time: %.2f seconds.\n", out_w, out_h, time_spent);
        
        printf("Saving the result in %s...\n", output_path);
        int stride_in_bytes = out_w * 3;
        if (!stbi_write_png(output_path, out_w, out_h, 3, output_pixels, stride_in_bytes)) {
            printf("[Error] Failed to save result.\n");
        } else {
            printf("Done!\n");
        }
        
        // Звільняємо буфер результату
        free(output_pixels);
    } else {
        printf("[Error] Upscale process failed.\n");
    }

    // 6. Фінальне очищення
    ai_upscaler_destroy(ctx);
    stbi_image_free(input_pixels);

    return 0;
}