#ifndef UPSCALE_H
#define UPSCALE_H

#include "libs/glfw/include/glad/glad.h"
#include "libs/glfw/include/GLFW/glfw3.h"

// Структура змінена під капотом, але API залишається зворотно сумісним
typedef struct {
    GLuint upscalePrograms[9]; // Масив окремих шейдерних програм для різних режимів (0-8)
    GLuint rcasProgram;        // Програма для пост-обробки (Pass 2)
    GLuint fboID;
    GLuint fboTexID;
    
    // --- Додаткові ресурси для Режиму 7 ---
    GLuint shader_denoise_mode7;
    GLuint shader_upscale_clean_mode7;
    GLuint shader_cas_sharpen_mode7;
    GLuint fbo_a;
    GLuint fbo_a_texture;
    GLuint fbo_b;
    GLuint fbo_b_texture;
    int fbo_a_last_width;
    int fbo_a_last_height;
    int fbo_b_last_width;
    int fbo_b_last_height;

    // --- Кешовані локації для Режиму 7 ---
    GLint denoise_mode7_texel_loc;
    GLint upscale_clean_mode7_texel_loc;
    GLint cas_sharpen_mode7_texel_loc;
    GLint cas_sharpen_mode7_textmode_loc;
    
    // Кешовані локації (тепер масив для першого проходу)
    GLint upscale_uTexelSizeLocs[9];
    
    // Локації для другого проходу
    GLint rcas_uTexelSizeLoc;
    GLint rcas_uSharpnessLoc;
    GLint rcas_uContrastLoc;
    GLint rcas_uBrightnessLoc;
    GLint rcas_uGammaLoc;
    GLint rcas_uTextModeLoc;
    GLint rcas_uZoomLevelLoc;
} UpscalePipeline;

// Сигнатури функцій залишилися ідентичними! (Зворотна сумісність)
int upscale_init_pipeline(UpscalePipeline *pipeline, int screen_width, int screen_height);
void upscale_render_pass(UpscalePipeline *pipeline, GLuint screen_texture, int filter_mode, float zoom_level, float offset_x, float offset_y, int screen_width, int screen_height, int flip_x, int flip_y, int original_width, int original_height);
void upscale_rcas_pass(UpscalePipeline *pipeline, int filter_mode, float contrast, float brightness, float gamma, int screen_width, int screen_height, float sharpness, int text_mode, float zoom_level);
void upscale_cleanup(UpscalePipeline *pipeline);

#endif // UPSCALE_H