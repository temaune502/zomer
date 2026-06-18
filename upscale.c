#include "upscale.h"
#define SHADER_IMPLEMENTATION
#include "shader.h"
#include <stdio.h>
#include <string.h>

// --- ФУНКЦІЇ КОМПІЛЯЦІЇ ТА ЛІНКУВАННЯ ШЕЙДЕРІВ ---
static unsigned int compile_shader(unsigned int type, const char *source) {
    unsigned int shader = glCreateShader(type);
    char log[1024]; int ok = 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        printf("Shader compile error: %s\n", log);
        glDeleteShader(shader); return 0;
    }
    return shader;
}

static unsigned int link_shader_program(const char *fragment_source) {
    unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, shader_vertex_source);
    unsigned int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    if (!vertex_shader || !fragment_shader) return 0;
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader); glDeleteShader(fragment_shader);
    int ok = 0; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(program); return 0; }
    return program;
}

static void draw_textured_quad(void) {
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
}

// --- ГОЛОВНІ ФУНКЦІЇ API ---
int upscale_init_pipeline(UpscalePipeline *pipeline, int screen_width, int screen_height) {
    memset(pipeline, 0, sizeof(UpscalePipeline));
    GLuint progBasic = link_shader_program(frag_basic_source);
    GLuint progCatmull = link_shader_program(frag_catmull_source);
    GLuint progFSR = link_shader_program(frag_fsr_source);
    GLuint progDenoise = link_shader_program(frag_denoise_source);
    GLuint progHybrid = link_shader_program(frag_hybrid_source);

    if (!progBasic || !progCatmull || !progFSR || !progDenoise || !progHybrid) return 0;

    pipeline->upscalePrograms[0] = progBasic; 
    pipeline->upscalePrograms[1] = progBasic; 
    pipeline->upscalePrograms[2] = progBasic; 
    pipeline->upscalePrograms[3] = progBasic; 
    pipeline->upscalePrograms[4] = progCatmull; 
    pipeline->upscalePrograms[5] = progFSR;
    pipeline->upscalePrograms[6] = progDenoise;
    pipeline->upscalePrograms[7] = progBasic; // Placeholder, we'll handle Mode7 separately
    pipeline->upscalePrograms[8] = progHybrid;

    for(int i = 0; i <= 8; i++) {
        glUseProgram(pipeline->upscalePrograms[i]);
        glUniform1i(glGetUniformLocation(pipeline->upscalePrograms[i], "screenTex"), 0);
        pipeline->upscale_uTexelSizeLocs[i] = glGetUniformLocation(pipeline->upscalePrograms[i], "uTexelSize");
    }
    glUseProgram(0);

    pipeline->rcasProgram = link_shader_program(post_fragment_source);
    if (!pipeline->rcasProgram) return 0;

    glUseProgram(pipeline->rcasProgram);
    glUniform1i(glGetUniformLocation(pipeline->rcasProgram, "screenTex"), 0);
    pipeline->rcas_uTexelSizeLoc = glGetUniformLocation(pipeline->rcasProgram, "uTexelSize");
    pipeline->rcas_uSharpnessLoc = glGetUniformLocation(pipeline->rcasProgram, "uSharpness");
    pipeline->rcas_uContrastLoc = glGetUniformLocation(pipeline->rcasProgram, "uContrast");
    pipeline->rcas_uBrightnessLoc = glGetUniformLocation(pipeline->rcasProgram, "uBrightness");
    pipeline->rcas_uGammaLoc = glGetUniformLocation(pipeline->rcasProgram, "uGammaValue");
    pipeline->rcas_uTextModeLoc = glGetUniformLocation(pipeline->rcasProgram, "uTextMode");
    pipeline->rcas_uZoomLevelLoc = glGetUniformLocation(pipeline->rcasProgram, "uZoomLevel");
    glUseProgram(0);

    // Initialize Mode7 shaders
    pipeline->shader_denoise_mode7 = link_shader_program(frag_denoise_source);
    pipeline->shader_upscale_clean_mode7 = link_shader_program(frag_fsr_no_flip_source);
    pipeline->shader_cas_sharpen_mode7 = link_shader_program(frag_cas_sharpen_source);
    if (!pipeline->shader_denoise_mode7 || !pipeline->shader_upscale_clean_mode7 || !pipeline->shader_cas_sharpen_mode7) {
        return 0;
    }
    // Set uniform locations for Mode7 shaders
    glUseProgram(pipeline->shader_denoise_mode7);
    glUniform1i(glGetUniformLocation(pipeline->shader_denoise_mode7, "screenTex"), 0);
    pipeline->denoise_mode7_texel_loc = glGetUniformLocation(pipeline->shader_denoise_mode7, "uTexelSize");

    glUseProgram(pipeline->shader_upscale_clean_mode7);
    glUniform1i(glGetUniformLocation(pipeline->shader_upscale_clean_mode7, "screenTex"), 0);
    pipeline->upscale_clean_mode7_texel_loc = glGetUniformLocation(pipeline->shader_upscale_clean_mode7, "uTexelSize");

    glUseProgram(pipeline->shader_cas_sharpen_mode7);
    glUniform1i(glGetUniformLocation(pipeline->shader_cas_sharpen_mode7, "screenTex"), 0);
    pipeline->cas_sharpen_mode7_texel_loc = glGetUniformLocation(pipeline->shader_cas_sharpen_mode7, "uTexelSize");
    pipeline->cas_sharpen_mode7_textmode_loc = glGetUniformLocation(pipeline->shader_cas_sharpen_mode7, "uTextMode");

    glUseProgram(0);

    // Initialize main FBO
    glGenFramebuffers(1, &pipeline->fboID);
    glGenTextures(1, &pipeline->fboTexID);
    glBindTexture(GL_TEXTURE_2D, pipeline->fboTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pipeline->fboTexID, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;
    
    // Initialize Mode7 FBOs (we'll set correct sizes later, but initialize with screen size first)
    glGenFramebuffers(1, &pipeline->fbo_a);
    glGenTextures(1, &pipeline->fbo_a_texture);
    glBindTexture(GL_TEXTURE_2D, pipeline->fbo_a_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fbo_a);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pipeline->fbo_a_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;

    glGenFramebuffers(1, &pipeline->fbo_b);
    glGenTextures(1, &pipeline->fbo_b_texture);
    glBindTexture(GL_TEXTURE_2D, pipeline->fbo_b_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fbo_b);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pipeline->fbo_b_texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return 0;

    // Initialize last sizes for FBOs
    pipeline->fbo_a_last_width = screen_width;
    pipeline->fbo_a_last_height = screen_height;
    pipeline->fbo_b_last_width = screen_width;
    pipeline->fbo_b_last_height = screen_height;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return 1;
}
void upscale_render_pass(UpscalePipeline *pipeline, GLuint screen_texture, int filter_mode, float zoom_level, float offset_x, float offset_y, int screen_width, int screen_height, int flip_x, int flip_y, int original_width, int original_height) {
    if (filter_mode == 7) {
        // --- РЕЖИМ 7: ТРИПРОХІДНИЙ КОНВЕЄР ---
        // Pass 1: Деноізинг в оригінальній роздільній здатності
        // Resize FBO A to original size only if it changed
        if (pipeline->fbo_a_last_width != original_width || pipeline->fbo_a_last_height != original_height) {
            glBindTexture(GL_TEXTURE_2D, pipeline->fbo_a_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, original_width, original_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            pipeline->fbo_a_last_width = original_width;
            pipeline->fbo_a_last_height = original_height;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fbo_a);
        glViewport(0, 0, original_width, original_height);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, screen_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        glUseProgram(pipeline->shader_denoise_mode7);
        glUniform2f(pipeline->denoise_mode7_texel_loc, 1.0f / (float)original_width, 1.0f / (float)original_height);
        
        draw_textured_quad();

        // Pass 2: Апскейл з FBO A в FBO B до розміру екрану
        // Resize FBO B to screen size only if it changed
        if (pipeline->fbo_b_last_width != screen_width || pipeline->fbo_b_last_height != screen_height) {
            glBindTexture(GL_TEXTURE_2D, pipeline->fbo_b_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            pipeline->fbo_b_last_width = screen_width;
            pipeline->fbo_b_last_height = screen_height;
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fbo_b);
        glViewport(0, 0, screen_width, screen_height);
        glClearColor(0.094f, 0.094f, 0.094f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(offset_x, offset_y, 0.0f);
        glScalef(zoom_level * (float)flip_x, zoom_level * (float)flip_y, 1.0f);

        glBindTexture(GL_TEXTURE_2D, pipeline->fbo_a_texture);
        // FBO A is already using getUV() which flips, so we don't want to double-flip!
        // So we need to make sure that when sampling from FBO A, we don't apply getUV() again!
        // Oops! That's the problem! Let's create a version of frag_fsr_source that doesn't flip for FBO A!
        // But wait, let's check: when we render to FBO A (Pass 1), the shader uses getUV() which flips screen_texture,
        // so the texture in FBO A is already flipped! So when we use FBO A in Pass 2, we should NOT use getUV()!
        // So let's make a version of the adaptive shader that doesn't flip!
        glUseProgram(pipeline->shader_upscale_clean_mode7);
        glUniform2f(pipeline->upscale_clean_mode7_texel_loc, 1.0f / (float)original_width, 1.0f / (float)original_height);

        draw_textured_quad();
        glUseProgram(0);
        return;
    }

    // Звичайний режим для інших фільтрів
    glBindFramebuffer(GL_FRAMEBUFFER, pipeline->fboID);
    glViewport(0, 0, screen_width, screen_height);
    glClearColor(0.094f, 0.094f, 0.094f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(offset_x, offset_y, 0.0f);
    glScalef(zoom_level * (float)flip_x, zoom_level * (float)flip_y, 1.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, screen_texture);
    
    int texture_filter = GL_LINEAR;
    if (filter_mode == 3 || filter_mode == 4 || filter_mode == 5 || filter_mode == 7 || filter_mode == 8) {
        texture_filter = GL_NEAREST;
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);

    int safe_mode = filter_mode;
    if (safe_mode < 0) safe_mode = 0;
    if (safe_mode > 8) safe_mode = 8;

    GLuint current_program = pipeline->upscalePrograms[safe_mode];
    
    if (current_program) {
        glUseProgram(current_program);
        if (pipeline->upscale_uTexelSizeLocs[safe_mode] != -1) {
            glUniform2f(pipeline->upscale_uTexelSizeLocs[safe_mode], 1.0f / (float)original_width, 1.0f / (float)original_height);
        }
    }

    draw_textured_quad();
    glUseProgram(0);
}

void upscale_rcas_pass(UpscalePipeline *pipeline, int filter_mode, float contrast, float brightness, float gamma, int screen_width, int screen_height, float sharpness, int text_mode, float zoom_level) {
    if (filter_mode == 7) {
        // Pass 3: Фінальна обробка з адаптивним шарпенням
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screen_width, screen_height);
        glClearColor(0.094f, 0.094f, 0.094f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, pipeline->fbo_b_texture);
        glUseProgram(pipeline->shader_cas_sharpen_mode7);
        glUniform2f(pipeline->cas_sharpen_mode7_texel_loc, 1.0f / (float)screen_width, 1.0f / (float)screen_height);
        glUniform1i(pipeline->cas_sharpen_mode7_textmode_loc, text_mode);
        
        draw_textured_quad();
        glUseProgram(0);
        glDisable(GL_TEXTURE_2D);
        return;
    }

    // Звичайна обробка для інших режимів
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_width, screen_height);
    glClearColor(0.094f, 0.094f, 0.094f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, pipeline->fboTexID);

    if (pipeline->rcasProgram) {
        glUseProgram(pipeline->rcasProgram);
        glUniform2f(pipeline->rcas_uTexelSizeLoc, 1.0f / (float)screen_width, 1.0f / (float)screen_height);
        glUniform1f(pipeline->rcas_uSharpnessLoc, sharpness);
        glUniform1f(pipeline->rcas_uContrastLoc, contrast);
        glUniform1f(pipeline->rcas_uBrightnessLoc, brightness);
        glUniform1f(pipeline->rcas_uGammaLoc, gamma);
        glUniform1i(pipeline->rcas_uTextModeLoc, text_mode);
        glUniform1f(pipeline->rcas_uZoomLevelLoc, zoom_level);
    }

    draw_textured_quad();
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
}

void upscale_cleanup(UpscalePipeline *pipeline) {
    GLuint deletedPrograms[9] = {0}; 
    for (int i = 0; i <= 8; i++) {
        GLuint p = pipeline->upscalePrograms[i];
        if (p) {
            int already_deleted = 0;
            for (int j = 0; j < i; j++) {
                if (deletedPrograms[j] == p) { already_deleted = 1; break; }
            }
            if (!already_deleted) {
                glDeleteProgram(p);
                deletedPrograms[i] = p;
            }
        }
    }
    
    if (pipeline->rcasProgram) glDeleteProgram(pipeline->rcasProgram);
    if (pipeline->shader_denoise_mode7) glDeleteProgram(pipeline->shader_denoise_mode7);
    if (pipeline->shader_upscale_clean_mode7) glDeleteProgram(pipeline->shader_upscale_clean_mode7);
    if (pipeline->shader_cas_sharpen_mode7) glDeleteProgram(pipeline->shader_cas_sharpen_mode7);
    if (pipeline->fboTexID) glDeleteTextures(1, &pipeline->fboTexID);
    if (pipeline->fboID) glDeleteFramebuffers(1, &pipeline->fboID);
    if (pipeline->fbo_a_texture) glDeleteTextures(1, &pipeline->fbo_a_texture);
    if (pipeline->fbo_a) glDeleteFramebuffers(1, &pipeline->fbo_a);
    if (pipeline->fbo_b_texture) glDeleteTextures(1, &pipeline->fbo_b_texture);
    if (pipeline->fbo_b) glDeleteFramebuffers(1, &pipeline->fbo_b);
}