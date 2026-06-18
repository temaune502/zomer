#ifndef AI_UPSCALER_H
#define AI_UPSCALER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    UPSCALER_EP_CPU = 0,
    UPSCALER_EP_DIRECTML = 1
} UpscalerProvider;

typedef struct {
    int tile_size;       // Розмір вхідного тайлу
    int scale_factor;    // Множник (tile_size * scale_factor не повинен перевищувати 4096)
    int channels;        // Тільки 3 (RGB, NCHW)
    UpscalerProvider ep;
    int dml_adapter_id;  // Індекс DXGI адаптера для DML
} UpscalerConfig;
typedef enum {
    UPSCALER_PRECISION_UNKNOWN = 0,
    UPSCALER_PRECISION_FP32 = 1,
    UPSCALER_PRECISION_FP16 = 2
} UpscalerPrecision;

typedef struct {
    int channels;                // Кількість каналів (наприклад, 3 для RGB)
    int scale_factor;            // Множник збільшення (або 0, якщо осі динамічні "-1")
    UpscalerPrecision precision; // Точність моделі (FP16 або FP32)
    bool is_valid;               // true, якщо інформацію успішно прочитано
} UpscalerModelInfo;


typedef struct UpscalerContext UpscalerContext;

#ifdef __cplusplus
extern "C" {
#endif
UpscalerContext* ai_upscaler_create_auto(const char* model_path, UpscalerProvider ep, int dml_adapter_id);
UpscalerContext* ai_upscaler_create(const char* model_path, const UpscalerConfig* config);
unsigned char* ai_upscaler_process(UpscalerContext* ctx, const unsigned char* in_pixels, int in_w, int in_h, int* out_w, int* out_h);
void ai_upscaler_destroy(UpscalerContext* ctx);
static inline uint16_t float_to_half_safe(float f);
static inline float half_to_float_safe(uint16_t h);

static bool ai_upscaler_read_cache(const char* model_path, int* out_scale, int* out_tile);
static void ai_upscaler_write_cache(const char* model_path, int scale, int tile, const char* description);
UpscalerModelInfo ai_upscaler_get_model_info(const char* model_path);

#ifdef __cplusplus
}
#endif

#endif // AI_UPSCALER_H

/* ========================================================================= */
/* --- IMPLEMENTATION ---                                                    */
/* ========================================================================= */

#ifdef AI_UPSCALER_IMPLEMENTATION

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "onnxruntime_portabl/include/onnxruntime_c_api.h"

#ifdef _WIN32
#include <wchar.h>
#include "onnxruntime_portabl/include/onnxruntime/core/providers/dml/dml_provider_factory.h"
#endif

struct UpscalerContext {
    UpscalerConfig config;
    const OrtApi* ort;
    OrtEnv* env;
    OrtSession* session;
    OrtMemoryInfo* memory_info;
    OrtAllocator* allocator;
    
    float* fp32_tile_buf;
    uint16_t* fp16_tile_buf;
    
    ONNXTensorElementDataType model_input_type;
    ONNXTensorElementDataType model_output_type;

    char input_name[128];
    char output_name[128];
};

static bool ai_upscaler_read_cache(const char* model_path, int* out_scale, int* out_tile) {
    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s.cache", model_path);
    
    FILE* f = fopen(cache_path, "r");
    if (!f) return false; // Кешу немає
    
    int items = fscanf(f, "scale_factor=%d\ntile_size=%d\n", out_scale, out_tile);
    fclose(f);
    
    // Перевіряємо, чи прочитано обидва значення і чи вони адекватні
    return (items == 2 && *out_scale > 0 && *out_tile > 0); 
}

// Читає опис моделі з кешу
static bool ai_upscaler_read_description(const char* model_path, char* out_desc, size_t max_desc_len) {
    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s.cache", model_path);
    
    FILE* f = fopen(cache_path, "r");
    if (!f) return false;
    
    // Пропускаємо перші два рядки (scale_factor та tile_size)
    char line[256];
    for (int i = 0; i < 2; i++) {
        if (!fgets(line, sizeof(line), f)) {
            fclose(f);
            return false;
        }
    }
    
    // Читаємо опис (якщо він є)
    bool found = false;
    if (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "description=", 12) == 0) {
            // Копіюємо текст після "description="
            strncpy(out_desc, line + 12, max_desc_len - 1);
            out_desc[max_desc_len - 1] = '\0';
            
            // Видаляємо новий рядок в кінці
            size_t len = strlen(out_desc);
            if (len > 0 && out_desc[len - 1] == '\n') {
                out_desc[len - 1] = '\0';
            }
            
            found = true;
        }
    }
    
    fclose(f);
    return found;
}

// Записує параметри у кеш-файл (з опціональним описом)
static void ai_upscaler_write_cache(const char* model_path, int scale, int tile, const char* description) {
    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s.cache", model_path);
    
    FILE* f = fopen(cache_path, "w");
    if (f) {
        fprintf(f, "scale_factor=%d\ntile_size=%d\n", scale, tile);
        if (description && description[0] != '\0') {
            fprintf(f, "description=%s\n", description);
        }
        fclose(f);
        printf("[Auto-Init] Saved optimal parameters to cache: %s\n", cache_path);
    } else {
        printf("[Auto-Init] Warning: Could not write cache file to %s\n", cache_path);
    }
}
//EXPEREMENTAL USE IN DEVELOPMENT (MAY CHANGE)
//CREATE UPSCALER CONTEXT FROM MODEL PATH AND DEFAULT CONFIG 
UpscalerContext* ai_upscaler_create_auto(const char* model_path, UpscalerProvider ep, int dml_adapter_id) {
    if (!model_path) return NULL;

    UpscalerConfig config;
    config.ep = ep;
    config.dml_adapter_id = dml_adapter_id;
    config.channels = 3; // Дефолт (перевизначимо, якщо треба)

    printf("[Auto-Init] Loading model: %s\n", model_path);

    // --- КРОК 1: ПЕРЕВІРКА КЕШУ ---
    int cached_scale = 0, cached_tile = 0;
    if (ai_upscaler_read_cache(model_path, &cached_scale, &cached_tile)) {
        config.scale_factor = cached_scale;
        config.tile_size = cached_tile;
        printf("[Auto-Init] SUCCESS: Loaded from cache! (Scale: %dx, Tile: %d)\n", config.scale_factor, config.tile_size);
    } 
    // --- КРОК 2: ЯКЩО КЕШУ НЕМАЄ, РОБИМО ПОВНИЙ АНАЛІЗ ---
    else {
        printf("[Auto-Init] Cache not found. Running full analysis...\n");
        UpscalerModelInfo info = ai_upscaler_get_model_info(model_path);
        
        if (!info.is_valid) {
            printf("[Auto-Init] Error: Failed to read model metadata.\n");
            return NULL;
        }

        config.channels = (info.channels > 0) ? info.channels : 3;
        if (config.channels != 3) {
            printf("[Auto-Init] Error: Model requires %d channels, strict RGB (3) supported.\n", config.channels);
            return NULL;
        }

        // СЦЕНАРІЙ А: Статична модель
        if (info.scale_factor > 0) {
            config.scale_factor = info.scale_factor;
            config.tile_size = 256; // Для статичних моделей ставимо безпечний розмір
            printf("[Auto-Init] Detected strict %dx scale factor from metadata.\n", config.scale_factor);
        } 
        // СЦЕНАРІЙ Б: Динамічна модель (Dry-Run)
        else {
            printf("[Auto-Init] Model axes are dynamic. Running cascading dry-run...\n");
            config.scale_factor = 0; 
            
            const OrtApi* ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
            if (ort) {
                OrtEnv* env = NULL; OrtSession* session = NULL; OrtSessionOptions* session_options = NULL; OrtMemoryInfo* memory_info = NULL;
                ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "AI_Upscaler_DryRun", &env);
                ort->CreateSessionOptions(&session_options);
                ort->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault, &memory_info);

#ifdef _WIN32
                wchar_t w_model_path[512];
                if (mbstowcs(w_model_path, model_path, 511) != (size_t)-1) ort->CreateSession(env, w_model_path, session_options, &session);
#else
                ort->CreateSession(env, model_path, session_options, &session);
#endif
                ort->ReleaseSessionOptions(session_options);

                if (session) {
                    const int possible_tiles[] = {1024, 512, 256, 128};
                    bool dry_run_success = false;

                    for (int i = 0; i < 4; i++) {
                        int test_size = possible_tiles[i];
                        printf("            Probing max tile size: %d... ", test_size);

                        int64_t input_shape[] = {1, config.channels, test_size, test_size};
                        size_t test_pixels = (size_t)test_size * test_size * config.channels;
                        OrtValue* input_tensor = NULL; OrtStatus* status = NULL; void* test_buf = NULL;

                        if (info.precision == UPSCALER_PRECISION_FP16) {
                            test_buf = calloc(test_pixels, sizeof(uint16_t));
                            status = ort->CreateTensorWithDataAsOrtValue(memory_info, test_buf, test_pixels * sizeof(uint16_t), input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, &input_tensor);
                        } else {
                            test_buf = calloc(test_pixels, sizeof(float));
                            status = ort->CreateTensorWithDataAsOrtValue(memory_info, test_buf, test_pixels * sizeof(float), input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensor);
                        }

                        if (!status && input_tensor) {
                            OrtAllocator* allocator; ort->GetAllocatorWithDefaultOptions(&allocator);
                            char* in_name = NULL; char* out_name = NULL;
                            ort->SessionGetInputName(session, 0, allocator, &in_name);
                            ort->SessionGetOutputName(session, 0, allocator, &out_name);
                            const char* input_names[] = {in_name}; const char* output_names[] = {out_name};
                            OrtValue* output_tensor = NULL;

                            status = ort->Run(session, NULL, input_names, (const OrtValue* const*)&input_tensor, 1, output_names, 1, &output_tensor);

                            if (in_name) allocator->Free(allocator, in_name);
                            if (out_name) allocator->Free(allocator, out_name);

                            if (!status && output_tensor) {
                                OrtTensorTypeAndShapeInfo* shape_info = NULL;
                                ort->GetTensorTypeAndShape(output_tensor, &shape_info);
                                if (shape_info) {
                                    int64_t out_shape[4]; ort->GetDimensions(shape_info, out_shape, 4);
                                    ort->ReleaseTensorTypeAndShapeInfo(shape_info);
                                    if (out_shape[2] > 0) {
                                        config.scale_factor = (int)(out_shape[2] / test_size);
                                        config.tile_size = test_size;
                                        dry_run_success = true;
                                    }
                                }
                                ort->ReleaseValue(output_tensor);
                                printf("Success!\n");
                            } else {
                                printf("Failed.\n");
                                if (status) ort->ReleaseStatus(status);
                            }
                        } else {
                            printf("Allocation failed.\n");
                            if (status) ort->ReleaseStatus(status);
                        }

                        if (input_tensor) ort->ReleaseValue(input_tensor);
                        if (test_buf) free(test_buf); 
                        if (dry_run_success) break;
                    }

                    if (!dry_run_success) {
                        printf("[Auto-Init] WARNING: Dry-run failed completely. Using strict fallbacks.\n");
                        config.scale_factor = 4; config.tile_size = 256;
                    }
                    ort->ReleaseSession(session);
                }
                if (memory_info) ort->ReleaseMemoryInfo(memory_info);
                if (env) ort->ReleaseEnv(env);
            }
        }

        // --- КРОК 3: ЗАПИСУЄМО РЕЗУЛЬТАТИ В КЕШ ---
        // Якщо опис вже існує в кеші, зберігаємо його
        char existing_desc[512] = "";
        ai_upscaler_read_description(model_path, existing_desc, sizeof(existing_desc));
        
        ai_upscaler_write_cache(model_path, config.scale_factor, config.tile_size, existing_desc);
    }

    // Створюємо фінальний робочий контекст
    UpscalerContext* ctx = ai_upscaler_create(model_path, &config);
    
    if (ctx) {
        printf("[Auto-Init] Context ready for inference!\n");
    } else {
        printf("[Auto-Init] Failed to create Context.\n");
    }

    return ctx;
}

// Додаємо функцію для запису тільки опису (без зміни інших параметрів)
static void ai_upscaler_write_description(const char* model_path, const char* description) {
    char cache_path[1024];
    snprintf(cache_path, sizeof(cache_path), "%s.cache", model_path);
    
    // Спочатку читаємо існуючі параметри
    int scale = 0, tile = 0;
    bool has_cache = ai_upscaler_read_cache(model_path, &scale, &tile);
    
    // Записуємо все разом (старими параметрами + новим описом)
    ai_upscaler_write_cache(model_path, has_cache ? scale : 4, has_cache ? tile : 256, description);
}
UpscalerModelInfo ai_upscaler_get_model_info(const char* model_path) {
    UpscalerModelInfo info = {0, 0, UPSCALER_PRECISION_UNKNOWN, false};
    if (!model_path) return info;

    const OrtApiBase* api_base = OrtGetApiBase();
    if (!api_base) return info;
    
    const OrtApi* ort = api_base->GetApi(ORT_API_VERSION);
    if (!ort) return info;

    OrtEnv* env = NULL;
    OrtSession* session = NULL;
    OrtSessionOptions* session_options = NULL;

    OrtStatus* status = ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "AI_Upscaler_Info", &env);
    if (status) { ort->ReleaseStatus(status); return info; }

    status = ort->CreateSessionOptions(&session_options);
    if (status) { ort->ReleaseStatus(status); ort->ReleaseEnv(env); return info; }

    // Завантаження моделі (суто для читання метаданих)
#ifdef _WIN32
    wchar_t w_model_path[512];
    size_t len = mbstowcs(w_model_path, model_path, 511);
    if (len != (size_t)-1) {
        w_model_path[len] = L'\0';
        status = ort->CreateSession(env, w_model_path, session_options, &session);
    } else {
        ort->ReleaseSessionOptions(session_options);
        ort->ReleaseEnv(env);
        return info;
    }
#else
    status = ort->CreateSession(env, model_path, session_options, &session);
#endif

    ort->ReleaseSessionOptions(session_options);

    if (status) {
        ort->ReleaseStatus(status);
        ort->ReleaseEnv(env);
        return info; // Модель не знайдено або вона пошкоджена
    }

    info.is_valid = true; // Модель успішно завантажилась

    OrtTypeInfo* type_info = NULL;
    const OrtTensorTypeAndShapeInfo* tensor_info = NULL;
    
    // --- 1. АНАЛІЗ ВХОДУ (Кількість каналів та точність) ---
    status = ort->SessionGetInputTypeInfo(session, 0, &type_info);
    int64_t in_shape[4] = { -1, -1, -1, -1 };
    
    if (status == NULL && type_info != NULL) {
        if (ort->CastTypeInfoToTensorInfo(type_info, &tensor_info) == NULL) {
            // Отримуємо точність
            ONNXTensorElementDataType element_type;
            ort->GetTensorElementType(tensor_info, &element_type);
            if (element_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) {
                info.precision = UPSCALER_PRECISION_FP32;
            } else if (element_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                info.precision = UPSCALER_PRECISION_FP16;
            }

            // Отримуємо форму входу
            size_t num_dims;
            ort->GetDimensionsCount(tensor_info, &num_dims);
            if (num_dims >= 4) { // Очікуємо NCHW
                ort->GetDimensions(tensor_info, in_shape, 4);
                if (in_shape[1] > 0) {
                    info.channels = (int)in_shape[1];
                }
            }
        }
        ort->ReleaseTypeInfo(type_info);
    } else {
        if (status) ort->ReleaseStatus(status);
    }

    // --- 2. АНАЛІЗ ВИХОДУ (Вираховуємо множник) ---
    type_info = NULL;
    tensor_info = NULL;
    status = ort->SessionGetOutputTypeInfo(session, 0, &type_info);
    
    if (status == NULL && type_info != NULL) {
        if (ort->CastTypeInfoToTensorInfo(type_info, &tensor_info) == NULL) {
            size_t num_dims;
            ort->GetDimensionsCount(tensor_info, &num_dims);
            if (num_dims >= 4) {
                int64_t out_shape[4] = { -1, -1, -1, -1 };
                ort->GetDimensions(tensor_info, out_shape, 4);
                
                // Вираховуємо scale_factor (Висота виходу / Висота входу)
                // Можливо лише якщо обидва значення статичні (більші за 0)
                if (in_shape[2] > 0 && out_shape[2] > 0) {
                    info.scale_factor = (int)(out_shape[2] / in_shape[2]);
                } else if (in_shape[3] > 0 && out_shape[3] > 0) {
                    info.scale_factor = (int)(out_shape[3] / in_shape[3]); // Фолбек на ширину
                }
            }
        }
        ort->ReleaseTypeInfo(type_info);
    } else {
        if (status) ort->ReleaseStatus(status);
    }

    // Очищення пам'яті сесії
    ort->ReleaseSession(session);
    ort->ReleaseEnv(env);

    return info;
}

static inline uint16_t float_to_half_safe(float f) {
    union { float f; uint32_t u; } f32 = { f };
    uint32_t sign = (f32.u >> 16) & 0x8000;
    int32_t exponent = (int32_t)((f32.u >> 23) & 0xff) - 127;
    uint32_t mantissa = f32.u & 0x7fffff;
    
    // Обробка NaN та Infinity для FP32 -> FP16
    if (exponent == 128) {
        if (mantissa != 0) return (uint16_t)(sign | 0x7e00); // NaN
        return (uint16_t)(sign | 0x7c00); // Infinity
    }
    
    // Обробка занадто великих чисел (Overflow) -> Infinity
    if (exponent > 15) return (uint16_t)(sign | 0x7c00);
    
    // Обробка денормалізованих (субнормальних) чисел
    if (exponent < -14) {
        if (exponent < -24) return (uint16_t)sign; // Занадто мале навіть для денормалізованого
        mantissa |= 0x800000; // Додаємо прихований біт
        uint32_t shift = (uint32_t)(-14 - exponent);
        // Округлення до найближчого (Round to nearest) перед зсувом
        mantissa = (mantissa + (1 << (12 + shift))) >> (13 + shift);
        return (uint16_t)(sign | mantissa);
    }
    
    // Звичайні нормалізовані числа з правильним округленням (Round to nearest)
    uint32_t exp_bits = (uint32_t)(exponent + 15) << 10;
    mantissa = (mantissa + 0x1000) >> 13; // Округлення додаванням половини біта зсуву
    
    // Перевірка, чи не призвело округлення мантіси до переповнення експоненти
    if (mantissa & 0x0400) {
        mantissa = 0;
        exp_bits += 0x0400;
    }
    
    return (uint16_t)(sign | exp_bits | (mantissa & 0x03ff));
}

static inline float half_to_float_safe(uint16_t h) {
    union { float f; uint32_t u; } f32;
    uint32_t sign = (h & 0x8000) << 16;
    uint32_t exp_half = (h & 0x7c00) >> 10;
    uint32_t mantissa = h & 0x03ff;
    
    if (exp_half == 0) {
        if (mantissa == 0) {
            f32.u = sign; // Чистий нуль
            return f32.f;
        }
        // Денормалізоване FP16 стає нормалізованим FP32
        while ((mantissa & 0x0400) == 0) {
            mantissa <<= 1;
            exp_half--;
        }
        exp_half++;
        mantissa &= 0x03ff;
        uint32_t exp_f32 = (uint32_t)((int32_t)exp_half - 15 + 127);
        f32.u = sign | (exp_f32 << 23) | (mantissa << 13);
        return f32.f;
    } else if (exp_half == 31) {
        // Infinity або NaN
        if (mantissa != 0) f32.u = sign | 0x7fc00000; // NaN
        else f32.u = sign | 0x7f800000; // Infinity
        return f32.f;
    }
    
    uint32_t exp_f32 = exp_half - 15 + 127;
    f32.u = sign | (exp_f32 << 23) | (mantissa << 13);
    return f32.f;
}

// Оптимізоване віддзеркалення за O(1) без повільних циклів while
static inline int reflect_coord(int x, int size) {
    if (size <= 1) return 0;
    if (x < 0) x = -x - 1;
    
    int double_size = 2 * size;
    x = x % double_size;
    
    if (x >= size) {
        x = double_size - x - 1;
    }
    return x;
}
UpscalerContext* ai_upscaler_create(const char* model_path, const UpscalerConfig* config) {
    if (!model_path || !config) return NULL;
    
    if (strlen(model_path) >= 512) {
        printf("Error: model_path exceeds 511 characters.\n");
        return NULL;
    }

    if (config->channels != 3) {
        printf("Error: Only 3-channel (RGB) NCHW format is currently supported.\n");
        return NULL;
    }
    if (config->tile_size <= 0 || config->scale_factor <= 0) {
        printf("Error: tile_size and scale_factor must be positive.\n");
        return NULL;
    }
    if (config->tile_size * config->scale_factor > 4096) {
        printf("Error: tile_size * scale_factor exceeds maximum safety limit (4096).\n");
        return NULL;
    }

    UpscalerContext* ctx = (UpscalerContext*)calloc(1, sizeof(UpscalerContext));
    if (!ctx) return NULL;

    ctx->config = *config;
    ctx->ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!ctx->ort) goto cleanup;

    OrtStatus* status = ctx->ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "AI_Upscaler", &ctx->env);
    if (status) { ctx->ort->ReleaseStatus(status); goto cleanup; }

    OrtSessionOptions* session_options = NULL;
    status = ctx->ort->CreateSessionOptions(&session_options);
    if (status) { ctx->ort->ReleaseStatus(status); goto cleanup; }

    if (config->ep == UPSCALER_EP_DIRECTML) {
#ifdef _WIN32
        status = OrtSessionOptionsAppendExecutionProvider_DML(session_options, config->dml_adapter_id);
        if (status) {
            printf("Warning: DirectML EP failed on adapter %d. Falling back to CPU.\n", config->dml_adapter_id);
            ctx->ort->ReleaseStatus(status);
        }
#else
        printf("Warning: DirectML is Windows-only. Falling back to CPU.\n");
#endif
    }

    status = ctx->ort->CreateCpuMemoryInfo(OrtDeviceAllocator, OrtMemTypeDefault, &ctx->memory_info);
    if (status) { ctx->ort->ReleaseStatus(status); ctx->ort->ReleaseSessionOptions(session_options); goto cleanup; }

#ifdef _WIN32
    wchar_t w_model_path[512];
    size_t len = mbstowcs(w_model_path, model_path, 511);
    if (len == (size_t)-1) {
        printf("Error: Invalid multibyte string in model_path.\n");
        ctx->ort->ReleaseSessionOptions(session_options);
        goto cleanup;
    }
    w_model_path[len] = L'\0';
    status = ctx->ort->CreateSession(ctx->env, w_model_path, session_options, &ctx->session);
#else
    status = ctx->ort->CreateSession(ctx->env, model_path, session_options, &ctx->session);
#endif

    ctx->ort->ReleaseSessionOptions(session_options);
    if (status) {
        printf("Error: Failed to create ONNX session: %s\n", ctx->ort->GetErrorMessage(status));
        ctx->ort->ReleaseStatus(status);
        goto cleanup;
    }

    ctx->ort->GetAllocatorWithDefaultOptions(&ctx->allocator);
    char* alloc_name = NULL;
    
    if (ctx->ort->SessionGetInputName(ctx->session, 0, ctx->allocator, &alloc_name) == NULL) {
        strncpy(ctx->input_name, alloc_name, sizeof(ctx->input_name) - 1);
        ctx->input_name[sizeof(ctx->input_name) - 1] = '\0';
        ctx->allocator->Free(ctx->allocator, alloc_name);
    } else { strcpy(ctx->input_name, "input"); }

    if (ctx->ort->SessionGetOutputName(ctx->session, 0, ctx->allocator, &alloc_name) == NULL) {
        strncpy(ctx->output_name, alloc_name, sizeof(ctx->output_name) - 1);
        ctx->output_name[sizeof(ctx->output_name) - 1] = '\0';
        ctx->allocator->Free(ctx->allocator, alloc_name);
    } else { strcpy(ctx->output_name, "output"); }

    // Валідація типів та форми входу
    OrtTypeInfo* type_info = NULL;
    const OrtTensorTypeAndShapeInfo* tensor_info = NULL;
    
    status = ctx->ort->SessionGetInputTypeInfo(ctx->session, 0, &type_info);
    if (status == NULL && type_info != NULL) {
        status = ctx->ort->CastTypeInfoToTensorInfo(type_info, &tensor_info);
        if (status == NULL && tensor_info != NULL) {
            ctx->ort->GetTensorElementType(tensor_info, &ctx->model_input_type);
            
            if (ctx->model_input_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT &&
                ctx->model_input_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                printf("Error: Unsupported input type. Model must use FLOAT32 or FLOAT16.\n");
                ctx->ort->ReleaseTypeInfo(type_info);
                goto cleanup;
            }

            size_t num_dims;
            ctx->ort->GetDimensionsCount(tensor_info, &num_dims);
            if (num_dims == 4) {
                int64_t in_shape[4];
                ctx->ort->GetDimensions(tensor_info, in_shape, 4);
                if (in_shape[1] != -1 && in_shape[1] != 3) {
                    printf("Error: Model expects %lld channels, but 3 (RGB) is required.\n", (long long)in_shape[1]);
                    ctx->ort->ReleaseTypeInfo(type_info);
                    goto cleanup;
                }
                if (in_shape[2] != -1 && in_shape[2] != config->tile_size) {
                    printf("Warning: Model expected fixed H=%lld, config tile_size is %d.\n", (long long)in_shape[2], config->tile_size);
                }
                if (in_shape[3] != -1 && in_shape[3] != config->tile_size) {
                    printf("Warning: Model expected fixed W=%lld, config tile_size is %d.\n", (long long)in_shape[3], config->tile_size);
                }
            } else {
                printf("Error: Model input must be 4D (NCHW).\n");
                ctx->ort->ReleaseTypeInfo(type_info);
                goto cleanup;
            }
        }
        ctx->ort->ReleaseTypeInfo(type_info);
    } else {
        if (status) ctx->ort->ReleaseStatus(status);
        ctx->model_input_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    }

    // Валідація типів виходу
    status = ctx->ort->SessionGetOutputTypeInfo(ctx->session, 0, &type_info);
    if (status == NULL && type_info != NULL) {
        if (ctx->ort->CastTypeInfoToTensorInfo(type_info, &tensor_info) == NULL) {
            ctx->ort->GetTensorElementType(tensor_info, &ctx->model_output_type);
            
            if (ctx->model_output_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT &&
                ctx->model_output_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                printf("Error: Unsupported output type. Model must use FLOAT32 or FLOAT16.\n");
                ctx->ort->ReleaseTypeInfo(type_info);
                goto cleanup;
            }
        }
        ctx->ort->ReleaseTypeInfo(type_info);
    } else {
        if (status) ctx->ort->ReleaseStatus(status);
        ctx->model_output_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    }

    size_t max_tile_pixels = (size_t)config->tile_size * config->tile_size * 3;
    if (ctx->model_input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
        ctx->fp16_tile_buf = (uint16_t*)calloc(max_tile_pixels, sizeof(uint16_t));
        if (!ctx->fp16_tile_buf) goto cleanup;
    } else {
        ctx->fp32_tile_buf = (float*)calloc(max_tile_pixels, sizeof(float));
        if (!ctx->fp32_tile_buf) goto cleanup;
    }

    return ctx;

cleanup:
    ai_upscaler_destroy(ctx);
    return NULL;
}

unsigned char* ai_upscaler_process(UpscalerContext* ctx, const unsigned char* in_pixels, int in_w, int in_h, int* out_w, int* out_h) {
    if (!ctx || !in_pixels || !out_w || !out_h) return NULL;
    if (in_w <= 0 || in_h <= 0) return NULL;

    const int t_size = ctx->config.tile_size;
    const int scale = ctx->config.scale_factor;
    const int channels = 3;

    if (in_w > INT_MAX / scale) return NULL;
    if (in_h > INT_MAX / scale) return NULL;

    *out_w = in_w * scale;
    *out_h = in_h * scale;
    
    size_t pixels;
    if ((size_t)(*out_w) > SIZE_MAX / (size_t)(*out_h)) return NULL;
    pixels = (size_t)(*out_w) * (size_t)(*out_h);
    if (pixels > SIZE_MAX / channels) return NULL;
    
    size_t out_bytes = pixels * channels;

    unsigned char* final_out_pixels = (unsigned char*)calloc(1, out_bytes);
    if (!final_out_pixels) return NULL;

    size_t max_tile_pixels = (size_t)t_size * t_size * channels;
    const size_t plane_size = (size_t)t_size * t_size;
    int64_t input_shape[] = {1, channels, t_size, t_size};
    
    const char* input_names[] = {ctx->input_name};
    const char* output_names[] = {ctx->output_name};

    OrtValue* input_tensor = NULL;
    OrtStatus* status = NULL;
    
    if (ctx->model_input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
        status = ctx->ort->CreateTensorWithDataAsOrtValue(
            ctx->memory_info, ctx->fp16_tile_buf, max_tile_pixels * sizeof(uint16_t),
            input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16, &input_tensor);
    } else {
        status = ctx->ort->CreateTensorWithDataAsOrtValue(
            ctx->memory_info, ctx->fp32_tile_buf, max_tile_pixels * sizeof(float),
            input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input_tensor);
    }

    if (status) {
        ctx->ort->ReleaseStatus(status);
        free(final_out_pixels);
        return NULL;
    }

    for (int y = 0; y < in_h; y += t_size) {
        int actual_y = (y + t_size > in_h) ? (in_h - t_size < 0 ? 0 : in_h - t_size) : y;

        for (int x = 0; x < in_w; x += t_size) {
            int actual_x = (x + t_size > in_w) ? (in_w - t_size < 0 ? 0 : in_w - t_size) : x;

            int valid_w = (in_w - actual_x < t_size) ? (in_w - actual_x) : t_size;
            int valid_h = (in_h - actual_y < t_size) ? (in_h - actual_y) : t_size;

            for (int th = 0; th < t_size; th++) {
                for (int tw = 0; tw < t_size; tw++) {
                    int g_y = reflect_coord(actual_y + th, in_h);
                    int g_x = reflect_coord(actual_x + tw, in_w);

                    size_t in_idx = ((size_t)g_y * in_w + g_x) * channels;
                    size_t base_idx = (size_t)th * t_size + tw;

                    float r = (float)in_pixels[in_idx + 0] / 255.0f;
                    float g = (float)in_pixels[in_idx + 1] / 255.0f;
                    float b = (float)in_pixels[in_idx + 2] / 255.0f;

                    if (ctx->model_input_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                        ctx->fp16_tile_buf[base_idx] = float_to_half_safe(r);
                        ctx->fp16_tile_buf[base_idx + plane_size] = float_to_half_safe(g);
                        ctx->fp16_tile_buf[base_idx + plane_size * 2] = float_to_half_safe(b);
                    } else {
                        ctx->fp32_tile_buf[base_idx] = r;
                        ctx->fp32_tile_buf[base_idx + plane_size] = g;
                        ctx->fp32_tile_buf[base_idx + plane_size * 2] = b;
                    }
                }
            }

            OrtValue* output_tensor = NULL;
            status = ctx->ort->Run(ctx->session, NULL, input_names, (const OrtValue* const*)&input_tensor, 1, output_names, 1, &output_tensor);
            
            if (status != NULL) { 
                printf("Runtime Error during inference: %s\n", ctx->ort->GetErrorMessage(status));
                ctx->ort->ReleaseStatus(status); 
                goto process_cleanup; 
            }

            OrtTensorTypeAndShapeInfo* shape_info = NULL;
            status = ctx->ort->GetTensorTypeAndShape(output_tensor, &shape_info);
            if (status != NULL) {
                printf("Error getting output shape info: %s\n", ctx->ort->GetErrorMessage(status));
                ctx->ort->ReleaseStatus(status);
                ctx->ort->ReleaseValue(output_tensor);
                goto process_cleanup;
            }
            
            size_t dim_count;
            status = ctx->ort->GetDimensionsCount(shape_info, &dim_count);
            if (status != NULL || dim_count != 4) {
                if (status) {
                    printf("Error getting dimensions count: %s\n", ctx->ort->GetErrorMessage(status));
                    ctx->ort->ReleaseStatus(status);
                } else {
                    printf("Error: Output tensor is not 4D (NCHW). Dim count: %zu\n", dim_count);
                }
                ctx->ort->ReleaseTensorTypeAndShapeInfo(shape_info);
                ctx->ort->ReleaseValue(output_tensor);
                goto process_cleanup;
            }

            int64_t out_shape[4];
            status = ctx->ort->GetDimensions(shape_info, out_shape, 4);
            ctx->ort->ReleaseTensorTypeAndShapeInfo(shape_info);

            if (status != NULL) {
                printf("Error getting dimensions: %s\n", ctx->ort->GetErrorMessage(status));
                ctx->ort->ReleaseStatus(status);
                ctx->ort->ReleaseValue(output_tensor);
                goto process_cleanup;
            }

            // Сувора перевірка на точний збіг після інференсу
            if (out_shape[0] != 1) {
                printf("Error: Output batch size mismatch. Expected 1, got %lld\n", (long long)out_shape[0]);
                ctx->ort->ReleaseValue(output_tensor); goto process_cleanup;
            }
            if (out_shape[1] != channels) {
                printf("Error: Output channels mismatch. Expected %d, got %lld\n", channels, (long long)out_shape[1]);
                ctx->ort->ReleaseValue(output_tensor); goto process_cleanup;
            }
            if (out_shape[2] != (int64_t)t_size * scale) {
                printf("Error: Output height mismatch. Expected %d, got %lld\n", t_size * scale, (long long)out_shape[2]);
                ctx->ort->ReleaseValue(output_tensor); goto process_cleanup;
            }
            if (out_shape[3] != (int64_t)t_size * scale) {
                printf("Error: Output width mismatch. Expected %d, got %lld\n", t_size * scale, (long long)out_shape[3]);
                ctx->ort->ReleaseValue(output_tensor); goto process_cleanup;
            }

            const int t_out_w = t_size * scale;
            const size_t out_plane_size = (size_t)t_out_w * t_out_w;

            void* raw_out_data = NULL;
            ctx->ort->GetTensorMutableData(output_tensor, &raw_out_data);

            if (raw_out_data != NULL) {
                for (int th = 0; th < valid_h * scale; th++) {
                    for (int tw = 0; tw < valid_w * scale; tw++) {
                        int go_y = (actual_y * scale) + th;
                        int go_x = (actual_x * scale) + tw;
                        
                        if (go_x >= *out_w || go_y >= *out_h) continue;
                        
                        size_t base_t_idx = (size_t)th * t_out_w + tw;
                        size_t fi_idx = ((size_t)go_y * (*out_w) + go_x) * channels;
                        
                        float r, g, b;
                        if (ctx->model_output_type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
                            uint16_t* fp16_out = (uint16_t*)raw_out_data;
                            r = half_to_float_safe(fp16_out[base_t_idx]);
                            g = half_to_float_safe(fp16_out[base_t_idx + out_plane_size]);
                            b = half_to_float_safe(fp16_out[base_t_idx + out_plane_size * 2]);
                        } else {
                            float* fp32_out = (float*)raw_out_data;
                            r = fp32_out[base_t_idx];
                            g = fp32_out[base_t_idx + out_plane_size];
                            b = fp32_out[base_t_idx + out_plane_size * 2];
                        }

                        r *= 255.0f; g *= 255.0f; b *= 255.0f;
                        
                        final_out_pixels[fi_idx + 0] = (unsigned char)(r < 0.0f ? 0 : (r > 255.0f ? 255 : (int)r));
                        final_out_pixels[fi_idx + 1] = (unsigned char)(g < 0.0f ? 0 : (g > 255.0f ? 255 : (int)g));
                        final_out_pixels[fi_idx + 2] = (unsigned char)(b < 0.0f ? 0 : (b > 255.0f ? 255 : (int)b));
                    }
                }
            }

            ctx->ort->ReleaseValue(output_tensor);
        }
    }

    if (input_tensor) ctx->ort->ReleaseValue(input_tensor);
    return final_out_pixels;

process_cleanup:
    if (input_tensor) ctx->ort->ReleaseValue(input_tensor);
    if (final_out_pixels) free(final_out_pixels);
    return NULL;
}

void ai_upscaler_destroy(UpscalerContext* ctx) {
    if (!ctx) return;
    
    if (ctx->ort) {
        if (ctx->session) ctx->ort->ReleaseSession(ctx->session);
        if (ctx->memory_info) ctx->ort->ReleaseMemoryInfo(ctx->memory_info);
        if (ctx->env) ctx->ort->ReleaseEnv(ctx->env);
    }
    
    if (ctx->fp32_tile_buf) free(ctx->fp32_tile_buf);
    if (ctx->fp16_tile_buf) free(ctx->fp16_tile_buf);
    
    free(ctx);
}

#endif // AI_UPSCALER_IMPLEMENTATION