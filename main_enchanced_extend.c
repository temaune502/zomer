#include <stdio.h>
#include <stdlib.h>

#define AI_UPSCALER_IMPLEMENTATION
#include "libs/ai_upscaler.h"

#include <windows.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "libs/glfw/include/glad/glad.h"
#define GLFW_INCLUDE_NONE
#include "libs/glfw/include/GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "libs/glfw/include/GLFW/glfw3native.h"

#define UTILS_IMPLEMENTATION
#include "utils.h"
#include "upscale.h"
#include "ui.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libs/stb_image_write.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "libs/stb_truetype.h"

#define MAX_MODELS 32
static int ai_model_count = 0;
static int ai_model_index = 0;
static char ai_model_names[MAX_MODELS][64];
static char ai_model_paths[MAX_MODELS][256];
static char ai_model_descriptions[MAX_MODELS][512]; // New: Descriptions for models
static int ai_current_model_used = -1; // Tracks which model generated the current AI pixels

static void scan_models(void) {
    WIN32_FIND_DATAA find_data;
    HANDLE hFind;
    char search_path[MAX_PATH];
    char models_folder[MAX_PATH];
    
    // Отримуємо шлях до папки програми
    GetModuleFileNameA(NULL, models_folder, MAX_PATH);
    char* last_slash = strrchr(models_folder, '\\');
    if (last_slash) *last_slash = '\0';
    
    // Формуємо шлях "models\*.onnx"
    strcpy(search_path, models_folder);
    strcat(search_path, "\\models\\*.onnx");
    
    ai_model_count = 0;
    
    // Створюємо папку якщо вона не існує
    char create_folder_path[MAX_PATH];
    strcpy(create_folder_path, models_folder);
    strcat(create_folder_path, "\\models");
    CreateDirectoryA(create_folder_path, NULL);
    
    hFind = FindFirstFileA(search_path, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (ai_model_count >= MAX_MODELS) break;
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                // Зберігаємо ім'я моделі (без .onnx)
                char* dot = strrchr(find_data.cFileName, '.');
                strncpy(ai_model_names[ai_model_count], find_data.cFileName, (dot ? (dot - find_data.cFileName) : 63));
                ai_model_names[ai_model_count][(dot ? (dot - find_data.cFileName) : 63)] = '\0';
                
                // Зберігаємо повний шлях
                strcpy(ai_model_paths[ai_model_count], models_folder);
                strcat(ai_model_paths[ai_model_count], "\\models\\");
                strncat(ai_model_paths[ai_model_count], find_data.cFileName, 255 - strlen(ai_model_paths[ai_model_count]));
                
                // Завантажуємо опис з кешу (якщо він існує)
                ai_model_descriptions[ai_model_count][0] = '\0';
                ai_upscaler_read_description(ai_model_paths[ai_model_count], 
                                              ai_model_descriptions[ai_model_count], 
                                              sizeof(ai_model_descriptions[ai_model_count]));
                
                ai_model_count++;
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
    
    // Якщо жодної моделі не знайдено, додаємо дефолтну (якщо вона є)
    if (ai_model_count == 0) {
        char default_model_path[MAX_PATH];
        strcpy(default_model_path, models_folder);
        strcat(default_model_path, "\\4xNomos8k_span_otf_strong_fp16_opset16.onnx");
        FILE* f = fopen(default_model_path, "rb");
        if (f) {
            fclose(f);
            strcpy(ai_model_names[0], "4xNomos8k");
            strcpy(ai_model_paths[0], default_model_path);
            ai_model_descriptions[0][0] = '\0';
            ai_upscaler_read_description(ai_model_paths[0], 
                                          ai_model_descriptions[0], 
                                          sizeof(ai_model_descriptions[0]));
            ai_model_count = 1;
        }
    }
    
    ai_model_index = 0;
}





#define HOTKEY_ID_WIN_Z 1
#define HOTKEY_ID_WIN_SHIFT_Z 2
#define HOTKEY_ID_WIN_CTRL_Z 3

enum {
    FILTER_OFF = 0,
    FILTER_SMOOTH = 1,
    FILTER_SHARPEN = 2,
    FILTER_PIXELATED = 3,
    FILTER_LANCZOS = 4,
    FILTER_FSR = 5,
    FILTER_DENOISE = 6,
    FILTER_RCAS = 7,
    FILTER_HYBRID = 8,
    FILTER_COUNT = 9
};

float zoom_level = 1.0f;
float offset_x = 0.0f;
float offset_y = 0.0f;
float flashlight_radius = 260.0f;

int window_visible = 0;
unsigned int textureID = 0;
static UpscalePipeline upscale_pipeline = {0};
unsigned char* pixeldata_ptr = NULL;
int screen_width = 0;
int screen_height = 0;
int filter_mode = FILTER_SHARPEN;
int text_mode = 0;
float image_contrast = 1.0f;
float image_brightness = 0.0f;
float image_gamma = 1.0f;

static unsigned char *ai_pixels_rgba = NULL;
static unsigned int ai_texture_id = 0;
static int ai_width = 0;
static int ai_height = 0;
static int ai_ready = 0;
static int ai_show = 0;
static int ai_processing = 0;
static int ai_failed = 0;
static HANDLE ai_thread_handle = NULL;
static volatile LONG ai_thread_done = 0;
static volatile LONG ai_thread_failed = 0;
static unsigned char *ai_pending_rgba = NULL;
static int ai_pending_width = 0;
static int ai_pending_height = 0;
static DWORD ai_start_time = 0;
static DWORD ai_processing_time_ms = 0;

static void ai_upscale_clear(void);
static void ai_upscale_button_activate(void);
static void ai_upscale_poll_result(void);
static void save_ai_upscaled_to_desktop(void);

HWND hotkey_hwnd = NULL;
HHOOK keyboard_hook = NULL;
int using_keyboard_hook = 0;

static GLFWwindow *window = NULL;
static int glfw_active = 0;
static int focus_frames_remaining = 0;
static int window_bordered = 0;
static int window_width = 0;
static int window_height = 0;
static int original_window_width = 0;
static int original_window_height = 0;

// Асинхронне збереження
static HANDLE save_thread_handle = NULL;
static volatile LONG save_thread_done = 0;
static unsigned char *save_pixels = NULL;
static int save_width = 0;
static int save_height = 0;

static inline float px_to_ndc_x(int px);
static inline float px_to_ndc_y(int py);
static void content_px_to_screen(int content_px, int content_py, int *screen_px, int *screen_py);
static void window_size_callback(GLFWwindow *win, int w, int h);

enum {
    DRAW_TOOL_PENCIL = 0,
    DRAW_TOOL_LINE,
    DRAW_TOOL_RECT,
    DRAW_TOOL_ELLIPSE,
    DRAW_TOOL_BUCKET,
    DRAW_TOOL_ERASER,
    DRAW_TOOL_PIPETTE,
    DRAW_TOOL_COUNT
};

#define HUB_MARGIN_LEFT  12
#define HUB_WIDTH        80
#define HUB_PAD          10
#define HUB_BTN_SIZE     34
#define HUB_BTN_GAP      6
#define HUB_CLR_SIZE     26
#define HUB_CLR_GAP      5
#define DRAW_UNDO_MAX    20
#define DRAW_COLOR_COUNT  9


static unsigned char *draw_pixels = NULL;
static unsigned int draw_tex_id = 0;
static int draw_hub_visible = 0;
static int draw_tool = DRAW_TOOL_PENCIL;
static int draw_color_idx = 0;
static unsigned char draw_custom_color[4] = {0, 0, 0, 255};
static int draw_use_custom_color = 0;
static int draw_dirty = 0;
static int draw_lmb_was = 0;
static int draw_stroke_active = 0;
static int draw_start_px = 0;
static int draw_start_py = 0;
static int draw_last_px = 0;
static int draw_last_py = 0;
static int draw_cur_px = 0;
static int draw_cur_py = 0;
static int draw_margin = 0;
static int draw_canvas_w = 0;
static int draw_canvas_h = 0;
static int draw_brush_size = 4;
static unsigned char *draw_undo_stack[DRAW_UNDO_MAX];
static int draw_undo_count = 0;
static int help_overlay_visible = 0;
static HFONT help_overlay_font = NULL;

static int selection_active = 0;
static int selection_start_x = 0;
static int selection_start_y = 0;
static int selection_end_x = 0;
static int selection_end_y = 0;
static int q_pressed = 0;

// Help text font
static unsigned char* font_data = NULL;
static stbtt_bakedchar cdata[96]; // ASCII 32..126
static GLuint font_texture = 0;
static int font_texture_width = 512;
static int font_texture_height = 512;

static void composite_draw_onto_rgba(unsigned char *dst, int dst_w, int dst_h, int content_x0, int content_y0);

static int init_font(void);
static void draw_text(float x, float y, const char* text);
static void cleanup_font(void);

static const unsigned char draw_palette[DRAW_COLOR_COUNT][4] = {
    {  0,   0,   0, 255},
    {255,   0,   0, 255},
    {  0, 200,   0, 255},
    {  0, 100, 255, 255},
    {255, 255,   0, 255},
    {255, 255, 255, 255},
    {255, 140,   0, 255},
    {180,   0, 255, 255}
};

typedef struct ByteBuffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} ByteBuffer;

static void png_write_callback(void *context, void *data, int size)
{
    ByteBuffer *buffer = (ByteBuffer*)context;
    size_t new_size;
    unsigned char *new_data;

    if (size <= 0)
        return;

    new_size = buffer->size + (size_t)size;
    if (new_size > buffer->capacity) {
        size_t new_capacity = buffer->capacity ? buffer->capacity * 2 : 65536;
        while (new_capacity < new_size)
            new_capacity *= 2;

        new_data = (unsigned char*)realloc(buffer->data, new_capacity);
        if (!new_data)
            return;

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, (size_t)size);
    buffer->size = new_size;
}

static unsigned char *capture_visible_frame_rgba(int width, int height)
{
    size_t row_size = (size_t)width * 4;
    size_t total_size = row_size * (size_t)height;
    unsigned char *raw = (unsigned char*)malloc(total_size);
    unsigned char *flipped = (unsigned char*)malloc(total_size);

    if (!raw || !flipped) {
        free(raw);
        free(flipped);
        return NULL;
    }

    glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, raw);

    for (int y = 0; y < height; y++) {
        memcpy(
            flipped + (size_t)y * row_size,
            raw + (size_t)(height - 1 - y) * row_size,
            row_size
        );
    }

    free(raw);
    return flipped;
}

static   void focus_capture_window(void)
{
    HWND hwnd;

    if (!window)
        return;

    hwnd = glfwGetWin32Window(window);
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, screen_width, screen_height, SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);
    glfwFocusWindow(window);
}



static void apply_filter_mode(void)
{
    int texture_filter = (filter_mode == FILTER_PIXELATED) ? GL_NEAREST : GL_LINEAR;
    if (!textureID)
        return;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);
}

static void ai_upscale_clear(void)
{
    if (ai_thread_handle) {
        WaitForSingleObject(ai_thread_handle, INFINITE);
        CloseHandle(ai_thread_handle);
        ai_thread_handle = NULL;
    }

    if (ai_texture_id) {
        glDeleteTextures(1, &ai_texture_id);
        ai_texture_id = 0;
    }

    free(ai_pixels_rgba);
    ai_pixels_rgba = NULL;
    ai_width = 0;
    ai_height = 0;
    ai_ready = 0;
    ai_show = 0;
    ai_processing = 0;
    ai_failed = 0;
    InterlockedExchange(&ai_thread_done, 0);
    InterlockedExchange(&ai_thread_failed, 0);
    free(ai_pending_rgba);
    ai_pending_rgba = NULL;
    ai_pending_width = 0;
    ai_pending_height = 0;
}

static unsigned char *rgba_to_rgb_copy(const unsigned char *rgba, int width, int height)
{
    size_t pixels;
    unsigned char *rgb;

    if (!rgba || width <= 0 || height <= 0)
        return NULL;

    if ((size_t)width > SIZE_MAX / (size_t)height)
        return NULL;

    pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / 3)
        return NULL;

    rgb = (unsigned char*)malloc(pixels * 3);
    if (!rgb)
        return NULL;

    for (size_t i = 0; i < pixels; i++) {
        rgb[i * 3 + 0] = rgba[i * 4 + 0];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }

    return rgb;
}

static unsigned char *rgb_to_rgba_copy(const unsigned char *rgb, int width, int height)
{
    size_t pixels;
    unsigned char *rgba;

    if (!rgb || width <= 0 || height <= 0)
        return NULL;

    if ((size_t)width > SIZE_MAX / (size_t)height)
        return NULL;

    pixels = (size_t)width * (size_t)height;
    if (pixels > SIZE_MAX / 4)
        return NULL;

    rgba = (unsigned char*)malloc(pixels * 4);
    if (!rgba)
        return NULL;

    for (size_t i = 0; i < pixels; i++) {
        rgba[i * 4 + 0] = rgb[i * 3 + 0];
        rgba[i * 4 + 1] = rgb[i * 3 + 1];
        rgba[i * 4 + 2] = rgb[i * 3 + 2];
        rgba[i * 4 + 3] = 255;
    }

    return rgba;
}

static int ai_texture_size_supported(int width, int height)
{
    GLint max_texture_size = 0;

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    if (max_texture_size <= 0)
        return 0;

    return width > 0 && height > 0 && width <= max_texture_size && height <= max_texture_size;
}

typedef struct AiUpscaleJob {
    unsigned char *input_rgb;
    int width;
    int height;
} AiUpscaleJob;

static DWORD WINAPI ai_upscale_worker_thread(LPVOID param)
{
    AiUpscaleJob *job = (AiUpscaleJob*)param;
    UpscalerContext *ctx = NULL;
    unsigned char *output_rgb = NULL;
    unsigned char *output_rgba = NULL;
    int out_w = 0;
    int out_h = 0;

    if (!job || !job->input_rgb || job->width <= 0 || job->height <= 0)
        goto fail;
    
    if (ai_model_count <= 0)
        goto fail;

    ctx = ai_upscaler_create_auto(ai_model_paths[ai_model_index], UPSCALER_EP_DIRECTML, 0);
    if (!ctx)
        goto fail;

    output_rgb = ai_upscaler_process(ctx, job->input_rgb, job->width, job->height, &out_w, &out_h);
    if (!output_rgb || out_w <= 0 || out_h <= 0)
        goto fail;

    output_rgba = rgb_to_rgba_copy(output_rgb, out_w, out_h);
    if (!output_rgba)
        goto fail;

    ai_pending_rgba = output_rgba;
    output_rgba = NULL;
    ai_pending_width = out_w;
    ai_pending_height = out_h;
    InterlockedExchange(&ai_thread_failed, 0);
    InterlockedExchange(&ai_thread_done, 1);

    if (ctx)
        ai_upscaler_destroy(ctx);
    free(job->input_rgb);
    free(job);
    free(output_rgb);
    free(output_rgba);
    return 0;

fail:
    if (ctx)
        ai_upscaler_destroy(ctx);
    if (job) {
        free(job->input_rgb);
        free(job);
    }
    free(output_rgb);
    free(output_rgba);
    InterlockedExchange(&ai_thread_failed, 1);
    InterlockedExchange(&ai_thread_done, 1);
    return 1;
}

static void ai_upscale_start_async(void)
{
    AiUpscaleJob *job;

    if (ai_processing || !pixeldata_ptr || screen_width <= 0 || screen_height <= 0)
        return;

    // Clear previous result if exists
    ai_upscale_clear();

    free(ai_pending_rgba);
    ai_pending_rgba = NULL;
    ai_pending_width = 0;
    ai_pending_height = 0;

    job = (AiUpscaleJob *)calloc(1, sizeof(AiUpscaleJob));
    if (!job) {
        ai_failed = 1;
        return;
    }

    job->width = screen_width;
    job->height = screen_height;
    job->input_rgb = rgba_to_rgb_copy(pixeldata_ptr, screen_width, screen_height);
    if (!job->input_rgb) {
        free(job);
        ai_failed = 1;
        return;
    }

    InterlockedExchange(&ai_thread_done, 0);
    InterlockedExchange(&ai_thread_failed, 0);
    ai_processing = 1;
    ai_failed = 0;
    ai_start_time = GetTickCount();

    ai_thread_handle = CreateThread(NULL, 0, ai_upscale_worker_thread, job, 0, NULL);
    if (!ai_thread_handle) {
        free(job->input_rgb);
        free(job);
        ai_processing = 0;
        ai_failed = 1;
        InterlockedExchange(&ai_thread_done, 0);
        InterlockedExchange(&ai_thread_failed, 1);
    }
}

static void ai_upscale_poll_result(void)
{
    if (!ai_processing || !InterlockedCompareExchange(&ai_thread_done, 0, 0))
        return;

    ai_processing_time_ms = GetTickCount() - ai_start_time;

    if (ai_thread_handle) {
        WaitForSingleObject(ai_thread_handle, INFINITE);
        CloseHandle(ai_thread_handle);
        ai_thread_handle = NULL;
    }

    ai_processing = 0;

    if (InterlockedCompareExchange(&ai_thread_failed, 0, 0) || !ai_pending_rgba ||
        !ai_texture_size_supported(ai_pending_width, ai_pending_height)) {
        free(ai_pending_rgba);
        ai_pending_rgba = NULL;
        ai_pending_width = 0;
        ai_pending_height = 0;
        ai_failed = 1;
        InterlockedExchange(&ai_thread_done, 0);
        InterlockedExchange(&ai_thread_failed, 0);
        return;
    }

    if (ai_texture_id) {
        glDeleteTextures(1, &ai_texture_id);
        ai_texture_id = 0;
    }
    free(ai_pixels_rgba);

    ai_pixels_rgba = ai_pending_rgba;
    ai_width = ai_pending_width;
    ai_height = ai_pending_height;
    ai_pending_rgba = NULL;
    ai_pending_width = 0;
    ai_pending_height = 0;

    glGenTextures(1, &ai_texture_id);
    if (!ai_texture_id) {
        free(ai_pixels_rgba);
        ai_pixels_rgba = NULL;
        ai_width = 0;
        ai_height = 0;
        ai_ready = 0;
        ai_show = 0;
        ai_failed = 1;
        InterlockedExchange(&ai_thread_done, 0);
        InterlockedExchange(&ai_thread_failed, 0);
        return;
    }

    glBindTexture(GL_TEXTURE_2D, ai_texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ai_width, ai_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ai_pixels_rgba);
    glBindTexture(GL_TEXTURE_2D, 0);

    ai_ready = 1;
    ai_show = 1;
    ai_failed = 0;
    ai_current_model_used = ai_model_index;
    InterlockedExchange(&ai_thread_done, 0);
    InterlockedExchange(&ai_thread_failed, 0);
}

static void ai_upscale_button_activate(void)
{
    if (ai_processing)
        return;

    // If we already have a result, check if the model changed
    if (ai_ready) {
        if (ai_model_index != ai_current_model_used) {
            // Model changed - run new generation
            ai_upscale_start_async();
        } else {
            // Same model - toggle view
            ai_show = !ai_show;
        }
        return;
    }

    ai_upscale_start_async();
}

static void save_screenshot_to_desktop(void)
{
    char *user_profile = getenv("USERPROFILE");
    char path[MAX_PATH];
    time_t now;
    struct tm *tm_now;

    if (!pixeldata_ptr || !user_profile)
        return;

    now = time(NULL);
    tm_now = localtime(&now);
    if (!tm_now)
        return;

    snprintf(
        path,
        sizeof(path),
        "%s\\Desktop\\zomer_%04d%02d%02d_%02d%02d%02d.png",
        user_profile,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec
    );

    {
        size_t frame_size = (size_t)screen_width * (size_t)screen_height * 4;
        unsigned char *frame = (unsigned char*)malloc(frame_size);

        if (!frame)
            return;

        memcpy(frame, pixeldata_ptr, frame_size);
        composite_draw_onto_rgba(frame, screen_width, screen_height, 0, 0);
        stbi_write_png(path, screen_width, screen_height, 4, frame, screen_width * 4);
        free(frame);
    }
}

static DWORD WINAPI save_image_worker_thread(LPVOID param)
{
    char *user_profile = getenv("USERPROFILE");
    char path[MAX_PATH];
    time_t now;
    struct tm *tm_now;

    if (!save_pixels || save_width <= 0 || save_height <= 0 || !user_profile)
        goto cleanup;

    now = time(NULL);
    tm_now = localtime(&now);
    if (!tm_now)
        goto cleanup;

    snprintf(
        path,
        sizeof(path),
        "%s\\Desktop\\zomer_ai_with_drawing_%04d%02d%02d_%02d%02d%02d.png",
        user_profile,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec);

    stbi_write_png(path, save_width, save_height, 4, save_pixels, save_width * 4);

cleanup:
    free(save_pixels);
    save_pixels = NULL;
    InterlockedExchange(&save_thread_done, 1);
    return 0;
}

static void save_ai_upscaled_with_drawing_to_desktop_async(void)
{
    char *user_profile = getenv("USERPROFILE");
    unsigned char *composite = NULL;

    if (!ai_ready || !ai_pixels_rgba || ai_width <= 0 || ai_height <= 0 || !user_profile)
        return;

    // Чекаємо, якщо попередній потік ще працює
    if (save_thread_handle) {
        WaitForSingleObject(save_thread_handle, INFINITE);
        CloseHandle(save_thread_handle);
        save_thread_handle = NULL;
    }
    free(save_pixels);
    save_pixels = NULL;
    InterlockedExchange(&save_thread_done, 0);

    // Копіюємо зображення для компонування
    size_t size = (size_t)ai_width * (size_t)ai_height * 4;
    composite = (unsigned char*)malloc(size);
    if (!composite)
        return;
    memcpy(composite, ai_pixels_rgba, size);

    // Наносимо малюнок (якщо він є)
    if (draw_pixels && draw_canvas_w > 0 && draw_canvas_h > 0) {
        // Обчислюємо масштаб (оригінальний розмір / розмір скріншоту)
        float scale_x = (float)ai_width / (float)screen_width;
        float scale_y = (float)ai_height / (float)screen_height;
        
        // Перебираємо пікселі малюнка та наносимо на зображення
        for (int dy = 0; dy < ai_height; dy++) {
            int src_y = (int)((float)dy / scale_y) + draw_margin;
            if (src_y < 0 || src_y >= draw_canvas_h)
                continue;
            for (int dx = 0; dx < ai_width; dx++) {
                int src_x = (int)((float)dx / scale_x) + draw_margin;
                if (src_x < 0 || src_x >= draw_canvas_w)
                    continue;
                
                size_t src_idx = ((size_t)src_y * draw_canvas_w + (size_t)src_x) * 4;
                unsigned char *src_pixel = draw_pixels + src_idx;
                if (src_pixel[3] == 0)
                    continue; // Пропускаємо прозорі пікселі

                size_t dst_idx = ((size_t)dy * ai_width + (size_t)dx) * 4;
                float alpha = (float)src_pixel[3] / 255.0f;
                float inv_alpha = 1.0f - alpha;

                composite[dst_idx + 0] = (unsigned char)((float)src_pixel[0] * alpha + (float)composite[dst_idx + 0] * inv_alpha);
                composite[dst_idx + 1] = (unsigned char)((float)src_pixel[1] * alpha + (float)composite[dst_idx + 1] * inv_alpha);
                composite[dst_idx + 2] = (unsigned char)((float)src_pixel[2] * alpha + (float)composite[dst_idx + 2] * inv_alpha);
                composite[dst_idx + 3] = (unsigned char)(fminf(255.0f, (float)src_pixel[3] + (float)composite[dst_idx + 3] * inv_alpha));
            }
        }
    }

    // Зберігаємо дані для потоку
    save_pixels = composite;
    save_width = ai_width;
    save_height = ai_height;

    // Запускаємо потік
    save_thread_handle = CreateThread(NULL, 0, save_image_worker_thread, NULL, 0, NULL);
}

static void save_ai_upscaled_with_drawing_to_desktop(void)
{
    // Тепер функція лише запускає асинхронне збереження
    save_ai_upscaled_with_drawing_to_desktop_async();
}

static void save_ai_upscaled_to_desktop(void)
{
    char *user_profile = getenv("USERPROFILE");
    char path[MAX_PATH];
    time_t now;
    struct tm *tm_now;

    if (!ai_ready || !ai_pixels_rgba || ai_width <= 0 || ai_height <= 0 || !user_profile)
        return;

    now = time(NULL);
    tm_now = localtime(&now);
    if (!tm_now)
        return;

    snprintf(
        path,
        sizeof(path),
        "%s\\Desktop\\zomer_ai_%04d%02d%02d_%02d%02d%02d.png",
        user_profile,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec);

    stbi_write_png(path, ai_width, ai_height, 4, ai_pixels_rgba, ai_width * 4);
}

static void save_visible_frame_to_desktop(void)
{
    char *user_profile = getenv("USERPROFILE");
    char path[MAX_PATH];
    time_t now;
    struct tm *tm_now;
    unsigned char *frame;

    if (!user_profile || !window)
        return;

    frame = capture_visible_frame_rgba(screen_width, screen_height);
    if (!frame)
        return;

    now = time(NULL);
    tm_now = localtime(&now);
    if (!tm_now) {
        free(frame);
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s\\Desktop\\zomer_view_%04d%02d%02d_%02d%02d%02d.png",
        user_profile,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec
    );

    stbi_write_png(path, screen_width, screen_height, 4, frame, screen_width * 4);
    free(frame);
}

static void save_selection_to_desktop(int x0, int y0, int x1, int y1) {
    char *user_profile = getenv("USERPROFILE");
    char path[MAX_PATH];
    time_t now;
    struct tm *tm_now;

    if (!user_profile)
        return;

    // Normalize coordinates
    int nx0 = x0 < x1 ? x0 : x1;
    int ny0 = y0 < y1 ? y0 : y1;
    int nx1 = x0 < x1 ? x1 : x0;
    int ny1 = y0 < y1 ? y1 : y0;
    int w = nx1 - nx0;
    int h = ny1 - ny0;

    if (w <= 0 || h <= 0)
        return;

    // Allocate buffer
    unsigned char *out = (unsigned char*)malloc(w * h * 4);
    if (!out)
        return;

    // Read pixels from screenshot
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = nx0 + x;
            int sy = ny0 + y;
            if (sx >= 0 && sx < screen_width && sy >= 0 && sy < screen_height) {
                size_t src_idx = ((size_t)sy * screen_width + sx) * 4;
                size_t dst_idx = ((size_t)y * w + x) * 4;
                out[dst_idx + 0] = pixeldata_ptr[src_idx + 0];
                out[dst_idx + 1] = pixeldata_ptr[src_idx + 1];
                out[dst_idx + 2] = pixeldata_ptr[src_idx + 2];
                out[dst_idx + 3] = pixeldata_ptr[src_idx + 3];
            } else {
                // Background color 0x181818
                size_t dst_idx = ((size_t)y * w + x) * 4;
                out[dst_idx + 0] = 0x18;
                out[dst_idx + 1] = 0x18;
                out[dst_idx + 2] = 0x18;
                out[dst_idx + 3] = 0xFF;
            }
        }
    }

    // Composite drawing onto this
    composite_draw_onto_rgba(out, w, h, nx0, ny0);

    now = time(NULL);
    tm_now = localtime(&now);
    if (!tm_now) {
        free(out);
        return;
    }

    snprintf(
        path,
        sizeof(path),
        "%s\\Desktop\\zomer_selection_%04d%02d%02d_%02d%02d%02d.png",
        user_profile,
        tm_now->tm_year + 1900,
        tm_now->tm_mon + 1,
        tm_now->tm_mday,
        tm_now->tm_hour,
        tm_now->tm_min,
        tm_now->tm_sec
    );
    stbi_write_png(path, w, h, 4, out, w * 4);
    free(out);
}

static int copy_selection_to_clipboard(int x0, int y0, int x1, int y1) {
    // Normalize coordinates
    int nx0 = x0 < x1 ? x0 : x1;
    int ny0 = y0 < y1 ? y0 : y1;
    int nx1 = x0 < x1 ? x1 : x0;
    int ny1 = y0 < y1 ? y1 : y0;
    int w = nx1 - nx0;
    int h = ny1 - ny0;
    if (w <= 0 || h <= 0)
        return 0;

    // Allocate buffer
    unsigned char *out = (unsigned char*)malloc(w * h * 4);
    if (!out)
        return 0;

    // Read pixels from screenshot
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sx = nx0 + x;
            int sy = ny0 + y;
            if (sx >= 0 && sx < screen_width && sy >= 0 && sy < screen_height) {
                size_t src_idx = ((size_t)sy * screen_width + sx) * 4;
                size_t dst_idx = ((size_t)y * w + x) * 4;
                out[dst_idx + 0] = pixeldata_ptr[src_idx + 0];
                out[dst_idx + 1] = pixeldata_ptr[src_idx + 1];
                out[dst_idx + 2] = pixeldata_ptr[src_idx + 2];
                out[dst_idx + 3] = pixeldata_ptr[src_idx + 3];
            } else {
                // Background color 0x181818
                size_t dst_idx = ((size_t)y * w + x) * 4;
                out[dst_idx + 0] = 0x18;
                out[dst_idx + 1] = 0x18;
                out[dst_idx + 2] = 0x18;
                out[dst_idx + 3] = 0xFF;
            }
        }
    }

    composite_draw_onto_rgba(out, w, h, nx0, ny0);

    // Now create BMP for clipboard (BGRA, bottom-up
    size_t dib_size = sizeof(BITMAPINFOHEADER) + w * h * 4;
    HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE, dib_size);
    if (!hDIB) {
        free(out);
        return 0;
    }
    BITMAPINFOHEADER *header = (BITMAPINFOHEADER*)GlobalLock(hDIB);
    if (!header) {
        GlobalFree(hDIB);
        free(out);
        return 0;
    }
    ZeroMemory(header, sizeof(BITMAPINFOHEADER));
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = w;
    header->biHeight = -h; // top-down
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;

    // Convert RGBA to BGRA
    unsigned char *pDIB = (unsigned char*)(header + 1);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            pDIB[idx * 4 + 0] = out[idx * 4 + 2]; // B
            pDIB[idx * 4 + 1] = out[idx * 4 + 1];  // G
            pDIB[idx * 4 + 2] = out[idx * 4 + 0];  // R
            pDIB[idx * 4 + 3] = out[idx * 4 + 3];
        }
    }
    GlobalUnlock(hDIB);

    // Create PNG too
    ByteBuffer png = {0};
    stbi_write_png_to_func(png_write_callback, &png, w, h, 4, out, w * 4);
    HGLOBAL hPNG = NULL;
    if (png.data && png.size > 0) {
        hPNG = GlobalAlloc(GMEM_MOVEABLE, png.size);
        if (hPNG) {
            void* pPNG = GlobalLock(hPNG);
            if (pPNG) {
                memcpy(pPNG, png.data, png.size);
                GlobalUnlock(hPNG);
            } else {
                GlobalFree(hPNG);
                hPNG = NULL;
            }
        }
    }
    free(png.data);
    free(out);

    if (!OpenClipboard(NULL)) {
        GlobalFree(hDIB);
        if (hPNG) GlobalFree(hPNG);
        return 0;
    }
    EmptyClipboard();
    UINT pngFormat = RegisterClipboardFormatA("PNG");
    if (SetClipboardData(CF_DIB, hDIB)) hDIB = NULL;
    if (hPNG && SetClipboardData(pngFormat, hPNG)) hPNG = NULL;
    CloseClipboard();
    if (hDIB) GlobalFree(hDIB);
    if (hPNG) GlobalFree(hPNG);
    return 1;
}

static inline float px_to_ndc_x(int px)
{
    return (float)px / (float)window_width * 2.0f - 1.0f;
}

static inline float px_to_ndc_y(int py)
{
    return 1.0f - (float)py / (float)window_height * 2.0f;
}

static void render_selection_rect(void) {
    if (!selection_active)
        return;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Convert content pixels to screen pixels
    int sx0, sy0, sx1, sy1;
    content_px_to_screen(selection_start_x, selection_start_y, &sx0, &sy0);
    content_px_to_screen(selection_end_x, selection_end_y, &sx1, &sy1);

    // Normalize screen pixels
    int nx0 = sx0 < sx1 ? sx0 : sx1;
    int ny0 = sy0 < sy1 ? sy0 : sy1;
    int nx1 = sx0 < sx1 ? sx1 : sx0;
    int ny1 = sy0 < sy1 ? sy1 : sy0;

    // Draw the area outside with semi-transparent black
    glColor4f(0,0,0,0.5f);
    // Left of selection
    glBegin(GL_QUADS);
        glVertex2f(px_to_ndc_x(0), px_to_ndc_y(0));
        glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(0));
        glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(window_height));
    glVertex2f(px_to_ndc_x(0), px_to_ndc_y(window_height));
glEnd();
// Right of selection
glBegin(GL_QUADS);
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(0));
    glVertex2f(px_to_ndc_x(window_width), px_to_ndc_y(0));
    glVertex2f(px_to_ndc_x(window_width), px_to_ndc_y(window_height));
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(window_height));
glEnd();
// Top of selection
glBegin(GL_QUADS);
    glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(0));
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(0));
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(ny0));
    glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(ny0));
glEnd();
// Bottom of selection
glBegin(GL_QUADS);
    glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(ny1));
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(ny1));
    glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(window_height));
    glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(window_height));
    glEnd();

    // Draw border
    glColor3f(1.0f,1.0f,1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(ny0));
        glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(ny0));
        glVertex2f(px_to_ndc_x(nx1), px_to_ndc_y(ny1));
        glVertex2f(px_to_ndc_x(nx0), px_to_ndc_y(ny1));
    glEnd();
    glLineWidth(1.0f);

    glDisable(GL_BLEND);
}

static int copy_visible_frame_to_clipboard(void)
{
    unsigned char *frame = capture_visible_frame_rgba(screen_width, screen_height);
    HGLOBAL dib_handle = NULL;
    HGLOBAL png_handle = NULL;
    BITMAPINFOHEADER *header;
    unsigned char *dib_pixels;
    ByteBuffer png = {0};
    size_t dib_size;
    int png_format;

    if (!frame)
        return 0;

    dib_size = sizeof(BITMAPINFOHEADER) + (size_t)screen_width * (size_t)screen_height * 4;
    dib_handle = GlobalAlloc(GMEM_MOVEABLE, dib_size);
    if (!dib_handle) {
        free(frame);
        return 0;
    }

    header = (BITMAPINFOHEADER*)GlobalLock(dib_handle);
    if (!header) {
        GlobalFree(dib_handle);
        free(frame);
        return 0;
    }

    ZeroMemory(header, sizeof(BITMAPINFOHEADER));
    header->biSize = sizeof(BITMAPINFOHEADER);
    header->biWidth = screen_width;
    header->biHeight = screen_height;
    header->biPlanes = 1;
    header->biBitCount = 32;
    header->biCompression = BI_RGB;

    dib_pixels = (unsigned char*)(header + 1);
    for (int y = 0; y < screen_height; y++) {
        unsigned char *dst_row = dib_pixels + (size_t)y * (size_t)screen_width * 4;
        unsigned char *src_row = frame + (size_t)(screen_height - 1 - y) * (size_t)screen_width * 4;

        for (int x = 0; x < screen_width; x++) {
            dst_row[x * 4 + 0] = src_row[x * 4 + 2];
            dst_row[x * 4 + 1] = src_row[x * 4 + 1];
            dst_row[x * 4 + 2] = src_row[x * 4 + 0];
            dst_row[x * 4 + 3] = src_row[x * 4 + 3];
        }
    }

    GlobalUnlock(dib_handle);

    stbi_write_png_to_func(png_write_callback, &png, screen_width, screen_height, 4, frame, screen_width * 4);
    if (png.data && png.size > 0) {
        png_handle = GlobalAlloc(GMEM_MOVEABLE, png.size);
        if (png_handle) {
            void *png_data = GlobalLock(png_handle);
            if (png_data) {
                memcpy(png_data, png.data, png.size);
                GlobalUnlock(png_handle);
            } else {
                GlobalFree(png_handle);
                png_handle = NULL;
            }
        }
    }

    free(png.data);
    free(frame);

    if (!OpenClipboard(NULL)) {
        GlobalFree(dib_handle);
        if (png_handle)
            GlobalFree(png_handle);
        return 0;
    }

    EmptyClipboard();
    if (SetClipboardData(CF_DIB, dib_handle))
        dib_handle = NULL;

    if (png_handle) {
        png_format = RegisterClipboardFormatA("PNG");
        if (png_format && SetClipboardData(png_format, png_handle))
            png_handle = NULL;
        else
            GlobalFree(png_handle);
    }

    CloseClipboard();
    if (dib_handle)
        GlobalFree(dib_handle);

    return 1;
}

static void draw_textured_quad(void)
{
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();
}

static inline float content_px_to_ndc_x(int px)
{
    return (float)px / (float)screen_width * 2.0f - 1.0f;
}

static inline float content_px_to_ndc_y(int py)
{
    return 1.0f - (float)py / (float)screen_height * 2.0f;
}

static void screen_px_to_content(int screen_px, int screen_py, int *content_px, int *content_py)
{
    // Переводимо піксели екрану в NDC, враховуючи поточний розмір вікна
    float mouse_gl_x = (float)screen_px / (float)screen_w * 2.0f - 1.0f;
    float mouse_gl_y = 1.0f - (float)screen_py / (float)screen_h * 2.0f;
    float safe_zoom = zoom_level;
    float cx, cy;

    if (safe_zoom < 0.0001f)
        safe_zoom = 0.0001f;

    cx = (mouse_gl_x - offset_x) / (safe_zoom * (float)flip_x);
    cy = (mouse_gl_y - offset_y) / (safe_zoom * (float)flip_y);

    *content_px = (int)(((cx + 1.0f) * 0.5f) * (float)screen_width + 0.5f);
    *content_py = (int)((1.0f - cy) * 0.5f * (float)screen_height + 0.5f);
}

static void window_size_callback(GLFWwindow *win, int w, int h)
{
    window_width = w;
    window_height = h;
    screen_w = w;
    screen_h = h;
}

static void content_px_to_screen(int content_px, int content_py, int *screen_px, int *screen_py)
{
    // Convert content px to NDC
    float cx = ((float)content_px / (float)screen_width) * 2.0f - 1.0f;
    float cy = 1.0f - ((float)content_py / (float)screen_height) * 2.0f;
    float safe_zoom = zoom_level;

    if (safe_zoom < 0.0001f)
        safe_zoom = 0.0001f;

    // Apply transform: screen_gl = offset + zoom * flip * content_gl
    float screen_gl_x = offset_x + (safe_zoom * (float)flip_x) * cx;
    float screen_gl_y = offset_y + (safe_zoom * (float)flip_y) * cy;

    // Convert back to screen pixels
    *screen_px = (int)(((screen_gl_x + 1.0f) * 0.5f) * (float)screen_w + 0.5f);
    *screen_py = (int)((1.0f - screen_gl_y) * 0.5f * (float)screen_h + 0.5f);
}

/* CPU-текстура: рядок 0 у пам'яті = верх контенту = GL texcoord v=0 */
static  void draw_content_textured_quad(int cx0, int cy0, int cx1, int cy1)
{
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(content_px_to_ndc_x(cx0), content_px_to_ndc_y(cy0));
        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(content_px_to_ndc_x(cx1), content_px_to_ndc_y(cy0));
        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(content_px_to_ndc_x(cx1), content_px_to_ndc_y(cy1));
        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(content_px_to_ndc_x(cx0), content_px_to_ndc_y(cy1));
    glEnd();
}

static inline void apply_content_view_transform(void)
{
    glLoadIdentity();
    glTranslatef(offset_x, offset_y, 0.0f);
    glScalef(zoom_level * (float)flip_x, zoom_level * (float)flip_y, 1.0f);
}

static inline size_t draw_canvas_bytes(void)
{
    return (size_t)draw_canvas_w * (size_t)draw_canvas_h * 4;
}

static inline void content_to_canvas(int cx, int cy, int *ox, int *oy)
{
    *ox = cx + draw_margin;
    *oy = cy + draw_margin;
}

static int draw_tool_brush_radius(void)
{
    if (draw_tool == DRAW_TOOL_ERASER)
        return draw_brush_size * 3;
    return draw_brush_size;
}

static void draw_undo_clear(void)
{
    for (int i = 0; i < draw_undo_count; i++)
        free(draw_undo_stack[i]);

    draw_undo_count = 0;
}

static void draw_undo_push(void)
{
    unsigned char *snap;
    size_t bytes;

    if (!draw_pixels || draw_canvas_w <= 0 || draw_canvas_h <= 0)
        return;

    bytes = draw_canvas_bytes();
    if (draw_undo_count >= DRAW_UNDO_MAX) {
        free(draw_undo_stack[0]);
        memmove(draw_undo_stack, draw_undo_stack + 1, (size_t)(draw_undo_count - 1) * sizeof(draw_undo_stack[0]));
        draw_undo_count--;
    }

    snap = (unsigned char*)malloc(bytes);
    if (!snap)
        return;

    memcpy(snap, draw_pixels, bytes);
    draw_undo_stack[draw_undo_count++] = snap;
}

static void draw_undo_pop(void)
{
    size_t bytes;

    if (!draw_pixels || draw_undo_count <= 0)
        return;

    draw_undo_count--;
    bytes = draw_canvas_bytes();
    memcpy(draw_pixels, draw_undo_stack[draw_undo_count], bytes);
    free(draw_undo_stack[draw_undo_count]);
    draw_dirty = 1;
}

static void draw_set_pixel(int x, int y, const unsigned char rgba[4])
{
    size_t idx;
    int ox, oy;

    if (!draw_pixels)
        return;

    content_to_canvas(x, y, &ox, &oy);
    if (ox < 0 || oy < 0 || ox >= draw_canvas_w || oy >= draw_canvas_h)
        return;

    idx = ((size_t)oy * (size_t)draw_canvas_w + (size_t)ox) * 4;
    draw_pixels[idx + 0] = rgba[0];
    draw_pixels[idx + 1] = rgba[1];
    draw_pixels[idx + 2] = rgba[2];
    draw_pixels[idx + 3] = rgba[3];
    draw_dirty = 1;
}

static void draw_brush(int cx, int cy, int radius, const unsigned char rgba[4], int erase)
{
    int x, y, dx, dy;
    unsigned char out[4];

    for (dy = -radius; dy <= radius; dy++) {
        for (dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy > radius * radius)
                continue;

            x = cx + dx;
            y = cy + dy;
            if (erase) {
                out[0] = out[1] = out[2] = out[3] = 0;
            } else {
                out[0] = rgba[0];
                out[1] = rgba[1];
                out[2] = rgba[2];
                out[3] = rgba[3];
            }
            draw_set_pixel(x, y, out);
        }
    }
}

static void draw_line_pixels(int x0, int y0, int x1, int y1, int radius, const unsigned char rgba[4], int erase)
{
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int x = x0;
    int y = y0;

    for (;;) {
        draw_brush(x, y, radius, rgba, erase);
        if (x == x1 && y == y1)
            break;

        {
            int e2 = err * 2;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }
}

static void draw_rect_outline(int x0, int y0, int x1, int y1, int radius, const unsigned char rgba[4], int erase)
{
    if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }

    draw_line_pixels(x0, y0, x1, y0, radius, rgba, erase);
    draw_line_pixels(x1, y0, x1, y1, radius, rgba, erase);
    draw_line_pixels(x1, y1, x0, y1, radius, rgba, erase);
    draw_line_pixels(x0, y1, x0, y0, radius, rgba, erase);
}

static void draw_ellipse_outline(int cx, int cy, int rx, int ry, int radius, const unsigned char rgba[4], int erase)
{
    int steps = rx + ry + 8;
    int i;
    int lx = cx + rx;
    int ly = cy;

    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;

    for (i = 1; i <= steps; i++) {
        float a = (float)i * 6.2831853f / (float)steps;
        int x = cx + (int)(rx * cosf(a));
        int y = cy + (int)(ry * sinf(a));
        draw_line_pixels(lx, ly, x, y, radius, rgba, erase);
        lx = x;
        ly = y;
    }
}

static void draw_flood_fill(int sx, int sy, const unsigned char fill[4])
{
    size_t stack_cap = 262144;
    int *stack_x = (int *)malloc(stack_cap * sizeof(int) * 2);
    size_t stack_size = 0;
    unsigned char target[4];
    size_t idx;
    int i;

    if (!draw_pixels || !stack_x)
        return;

    {
        int sox, soy;
        content_to_canvas(sx, sy, &sox, &soy);
        if (sox < 0 || soy < 0 || sox >= draw_canvas_w || soy >= draw_canvas_h)
            goto done;
        idx = ((size_t)soy * (size_t)draw_canvas_w + (size_t)sox) * 4;
    }
    target[0] = draw_pixels[idx + 0];
    target[1] = draw_pixels[idx + 1];
    target[2] = draw_pixels[idx + 2];
    target[3] = draw_pixels[idx + 3];

    if (target[0] == fill[0] && target[1] == fill[1] &&
        target[2] == fill[2] && target[3] == fill[3])
        goto done;

    stack_x[0] = sx;
    stack_x[1] = sy;
    stack_size = 1;

    while (stack_size > 0) {
        int px, py;
        size_t pidx;

        stack_size--;
        px = stack_x[stack_size * 2];
        py = stack_x[stack_size * 2 + 1];

        {
            int cox, coy;
            content_to_canvas(px, py, &cox, &coy);
            if (cox < 0 || coy < 0 || cox >= draw_canvas_w || coy >= draw_canvas_h)
                continue;
            pidx = ((size_t)coy * (size_t)draw_canvas_w + (size_t)cox) * 4;
        }
        if (draw_pixels[pidx + 0] != target[0] || draw_pixels[pidx + 1] != target[1] ||
            draw_pixels[pidx + 2] != target[2] || draw_pixels[pidx + 3] != target[3])
            continue;

        draw_pixels[pidx + 0] = fill[0];
        draw_pixels[pidx + 1] = fill[1];
        draw_pixels[pidx + 2] = fill[2];
        draw_pixels[pidx + 3] = fill[3];
        draw_dirty = 1;

        for (i = 0; i < 4; i++) {
            int nx = px + (i == 0 ? -1 : i == 1 ? 1 : 0);
            int ny = py + (i == 2 ? -1 : i == 3 ? 1 : 0);

            if (stack_size >= stack_cap)
                continue;

            stack_x[stack_size * 2] = nx;
            stack_x[stack_size * 2 + 1] = ny;
            stack_size++;
        }
    }

done:
    free(stack_x);
}

static void draw_clear_layer(void)
{
    if (!draw_pixels)
        return;

    draw_undo_push();
    memset(draw_pixels, 0, draw_canvas_bytes());
    draw_dirty = 1;
}

static int init_draw_layer(void)
{
    size_t bytes;

    draw_margin = 256*2;
    draw_canvas_w = screen_width + draw_margin * 2;
    draw_canvas_h = screen_height + draw_margin * 2;
    bytes = draw_canvas_bytes();

    draw_pixels = (unsigned char*)malloc(bytes);
    if (!draw_pixels)
        return 0;

    memset(draw_pixels, 0, bytes);
    draw_undo_clear();

    glGenTextures(1, &draw_tex_id);
    glBindTexture(GL_TEXTURE_2D, draw_tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, draw_canvas_w, draw_canvas_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, draw_pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    draw_hub_visible = 0;
    draw_tool = DRAW_TOOL_PENCIL;
    draw_color_idx = 0;
    draw_brush_size = 4;
    draw_dirty = 0;
    draw_lmb_was = 0;
    draw_stroke_active = 0;
    return 1;
}

static void destroy_draw_layer(void)
{
    if (draw_tex_id) {
        glDeleteTextures(1, &draw_tex_id);
        draw_tex_id = 0;
    }

    draw_undo_clear();
    free(draw_pixels);
    draw_pixels = NULL;
    draw_margin = 0;
    draw_canvas_w = 0;
    draw_canvas_h = 0;
    draw_hub_visible = 0;
    draw_dirty = 0;
    draw_stroke_active = 0;
    draw_lmb_was = 0;
}

static void upload_draw_texture(void)
{
    if (!draw_pixels || !draw_tex_id)
        return;

    glBindTexture(GL_TEXTURE_2D, draw_tex_id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, draw_canvas_w, draw_canvas_h, GL_RGBA, GL_UNSIGNED_BYTE, draw_pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    draw_dirty = 0;
}

static void render_draw_layer(void)
{
    if (!draw_tex_id)
        return;

    glMatrixMode(GL_MODELVIEW);
    apply_content_view_transform();
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, draw_tex_id);
    int texture_filter = (filter_mode == FILTER_PIXELATED) ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_content_textured_quad(-draw_margin, -draw_margin,
                               screen_width + draw_margin, screen_height + draw_margin);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void composite_draw_onto_rgba(unsigned char *dst, int dst_w, int dst_h, int content_x0, int content_y0)
{
    int x, y;

    if (!dst || !draw_pixels || draw_canvas_w <= 0 || draw_canvas_h <= 0)
        return;

    for (y = 0; y < dst_h; y++) {
        int cy = content_y0 + y;
        int oy = cy + draw_margin;
        unsigned char *dst_row = dst + (size_t)y * (size_t)dst_w * 4;

        if (oy < 0 || oy >= draw_canvas_h)
            continue;

        for (x = 0; x < dst_w; x++) {
            int cx = content_x0 + x;
            int ox = cx + draw_margin;
            size_t src_idx;
            float alpha;
            unsigned char *src;
            unsigned char *d;

            if (ox < 0 || ox >= draw_canvas_w)
                continue;

            src_idx = ((size_t)oy * (size_t)draw_canvas_w + (size_t)ox) * 4;
            src = draw_pixels + src_idx;
            if (src[3] == 0)
                continue;

            d = dst_row + (size_t)x * 4;
            alpha = (float)src[3] / 255.0f;
            d[0] = (unsigned char)((float)src[0] * alpha + (float)d[0] * (1.0f - alpha));
            d[1] = (unsigned char)((float)src[1] * alpha + (float)d[1] * (1.0f - alpha));
            d[2] = (unsigned char)((float)src[2] * alpha + (float)d[2] * (1.0f - alpha));
            d[3] = (unsigned char)(fminf(255.0f, (float)src[3] + (float)d[3] * (1.0f - alpha)));
        }
    }
}

#define HELP_PANEL_W 680
#define HELP_PANEL_H 700
#define HELP_LINE_H  20

static void draw_hub_layout(int *hub_x, int *hub_y, int *hub_w, int *hub_h)
{
    int tool_rows = (DRAW_TOOL_COUNT + 1) / 2;
    int tools_h = tool_rows * HUB_BTN_SIZE + (tool_rows - 1) * HUB_BTN_GAP;
    int color_rows = (DRAW_COLOR_COUNT + 1) / 2;
    int colors_h = color_rows * HUB_CLR_SIZE + (color_rows - 1) * HUB_CLR_GAP;
    int inner_h = HUB_PAD + tools_h + 18 + colors_h + 18 + 34 + 10 + 34 + 10 + 42 + HUB_PAD;

    *hub_w = HUB_WIDTH;
    *hub_h = inner_h;
    *hub_x = HUB_MARGIN_LEFT;
    *hub_y = (screen_h - inner_h) / 2;
    if (*hub_y < 8)
        *hub_y = 8;
}

static void icon_tool_cb(int x, int y, int w, int h, void* userdata) {
    int tool = (int)(intptr_t)userdata;
    int cx = x + w / 2;
    int cy = y + h / 2;

    glColor3f(0.95f, 0.96f, 1.0f);
    glLineWidth(2.2f);

    if (tool == DRAW_TOOL_PENCIL) {
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(x + 8), px_to_ndc_y(y + 26));
            glVertex2f(px_to_ndc_x(x + 24), px_to_ndc_y(y + 10));
        glEnd();
        // Drawing a small yellow square for pencil tip
        float x0 = px_to_ndc_x(x + 21);
        float y0 = px_to_ndc_y(y + 7);
        float x1 = px_to_ndc_x(x + 26);
        float y1 = px_to_ndc_y(y + 12);
        glColor3f(0.95f, 0.78f, 0.25f);
        glBegin(GL_QUADS);
            glVertex2f(x0, y1); glVertex2f(x1, y1);
            glVertex2f(x1, y0); glVertex2f(x0, y0);
        glEnd();
    } else if (tool == DRAW_TOOL_LINE) {
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(x + 7), px_to_ndc_y(y + 24));
            glVertex2f(px_to_ndc_x(x + 27), px_to_ndc_y(y + 10));
        glEnd();
        glPointSize(4.0f);
        glBegin(GL_POINTS);
            glVertex2f(px_to_ndc_x(x + 7), px_to_ndc_y(y + 24));
            glVertex2f(px_to_ndc_x(x + 27), px_to_ndc_y(y + 10));
        glEnd();
        glPointSize(1.0f);
    } else if (tool == DRAW_TOOL_RECT) {
        float x0 = px_to_ndc_x(x + 7);
        float y0 = px_to_ndc_y(y + 7);
        float x1 = px_to_ndc_x(x + 27);
        float y1 = px_to_ndc_y(y + 27);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x0, y1); glVertex2f(x1, y1);
            glVertex2f(x1, y0); glVertex2f(x0, y0);
        glEnd();
    } else if (tool == DRAW_TOOL_ELLIPSE) {
        glBegin(GL_LINE_LOOP);
        for (int s = 0; s < 32; s++) {
            float ang = s * 6.2831853f / 32.0f;
            glVertex2f(px_to_ndc_x(cx + (int)(11.0f * cosf(ang))),
                       px_to_ndc_y(cy + (int)(11.0f * sinf(ang))));
        }
        glEnd();
    } else if (tool == DRAW_TOOL_BUCKET) {
        float x0 = px_to_ndc_x(x + 9);
        float y0 = px_to_ndc_y(y + 14);
        float x1 = px_to_ndc_x(x + 25);
        float y1 = px_to_ndc_y(y + 26);
        glColor3f(0.55f, 0.62f, 0.82f);
        glBegin(GL_QUADS);
            glVertex2f(x0, y1); glVertex2f(x1, y1);
            glVertex2f(x1, y0); glVertex2f(x0, y0);
        glEnd();
        glColor3f(0.95f, 0.96f, 1.0f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(px_to_ndc_x(x + 9), px_to_ndc_y(y + 14));
            glVertex2f(px_to_ndc_x(x + 25), px_to_ndc_y(y + 14));
            glVertex2f(px_to_ndc_x(x + 23), px_to_ndc_y(y + 26));
            glVertex2f(px_to_ndc_x(x + 11), px_to_ndc_y(y + 26));
        glEnd();
    } else if (tool == DRAW_TOOL_ERASER) {
        float x0 = px_to_ndc_x(x + 8);
        float y0 = px_to_ndc_y(y + 11);
        float x1 = px_to_ndc_x(x + 26);
        float y1 = px_to_ndc_y(y + 24);
        glColor3f(0.92f, 0.70f, 0.78f);
        glBegin(GL_QUADS);
            glVertex2f(x0, y1); glVertex2f(x1, y1);
            glVertex2f(x1, y0); glVertex2f(x0, y0);
        glEnd();
        glColor3f(0.98f, 0.98f, 1.0f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(x0, y1); glVertex2f(x1, y1);
            glVertex2f(x1, y0); glVertex2f(x0, y0);
        glEnd();
    } else if (tool == DRAW_TOOL_PIPETTE) {
        glBegin(GL_LINE_STRIP);
            glVertex2f(px_to_ndc_x(x + 26), px_to_ndc_y(y + 8));
            glVertex2f(px_to_ndc_x(x + 12), px_to_ndc_y(y + 22));
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(x + 8), px_to_ndc_y(y + 22));
            glVertex2f(px_to_ndc_x(x + 14), px_to_ndc_y(y + 28));
        glEnd();
    }
    glLineWidth(1.0f);
}

static void icon_clear_cb(int x, int y, int w, int h, void* userdata) {
    glColor3f(1.0f, 0.85f, 0.85f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(px_to_ndc_x(x + 10), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 24), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 17), px_to_ndc_y(y + 10));
        glVertex2f(px_to_ndc_x(x + 17), px_to_ndc_y(y + 24));
    glEnd();
    glLineWidth(1.0f);
}

static void icon_ai_cb(int x, int y, int w, int h, void* userdata) {
    // Draw "AI" text or simple brain icon
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_text(px_to_ndc_x(x + 6), px_to_ndc_y(y + 10), "AI");
}

static void icon_arrow_cb(int x, int y, int w, int h, void* userdata) {
    int dir = (int)(intptr_t)userdata; // -1 for left, 1 for right
    glColor3f(1.0f, 1.0f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    if (dir < 0) {
        glVertex2f(px_to_ndc_x(x + 20), px_to_ndc_y(y + 10));
        glVertex2f(px_to_ndc_x(x + 10), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 10), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 20), px_to_ndc_y(y + 24));
    } else {
        glVertex2f(px_to_ndc_x(x + 10), px_to_ndc_y(y + 10));
        glVertex2f(px_to_ndc_x(x + 20), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 20), px_to_ndc_y(y + 17));
        glVertex2f(px_to_ndc_x(x + 10), px_to_ndc_y(y + 24));
    }
    glEnd();
    glLineWidth(1.0f);
}

static void render_and_handle_ui(GLFWwindow* win) {
    if (!draw_hub_visible) return;

    double mx, my;
    glfwGetCursorPos(win, &mx, &my);
    bool lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    ui_begin(screen_w, screen_h, (int)mx, (int)my, lmb);

    int hub_x, hub_y, hub_w, hub_h;
    draw_hub_layout(&hub_x, &hub_y, &hub_w, &hub_h);

    // Draw hub background
    ui_draw_rect(hub_x, hub_y, hub_w, hub_h, (UIColor){0.12f, 0.12f, 0.14f, 0.88f});
    ui_draw_rect_outline(hub_x, hub_y, hub_w, hub_h, (UIColor){0.45f, 0.45f, 0.50f, 1.0f}, 1.0f);

    int bx = hub_x + HUB_PAD;
    int by = hub_y + HUB_PAD;

    // Tools
    for (int i = 0; i < DRAW_TOOL_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int tx = bx + col * (HUB_BTN_SIZE + HUB_BTN_GAP);
        int ty = by + row * (HUB_BTN_SIZE + HUB_BTN_GAP);
        
        UIColor base_color = {0.28f, 0.28f, 0.32f, 0.95f};
        UIColor hover_color = {0.42f, 0.42f, 0.48f, 1.0f};
        
        if (ui_icon_button(10 + i, tx, ty, HUB_BTN_SIZE, HUB_BTN_SIZE, base_color, hover_color, draw_tool == i, icon_tool_cb, (void*)(intptr_t)i)) {
            draw_tool = i;
        }
    }

    int tool_rows = (DRAW_TOOL_COUNT + 1) / 2;
    by += tool_rows * (HUB_BTN_SIZE + HUB_BTN_GAP) - HUB_BTN_GAP + 18;

    // Colors
    for (int i = 0; i < DRAW_COLOR_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int cx = bx + col * (HUB_CLR_SIZE + HUB_CLR_GAP);
        int cy = by + row * (HUB_CLR_SIZE + HUB_CLR_GAP);
        
        const unsigned char* c = draw_palette[i];
        UIColor color = {c[0]/255.0f, c[1]/255.0f, c[2]/255.0f, 1.0f};
        
        if (ui_color_button(100 + i, cx, cy, HUB_CLR_SIZE, HUB_CLR_SIZE, color, draw_color_idx == i && !draw_use_custom_color)) {
            draw_color_idx = i;
            draw_use_custom_color = false;
        }
    }

    int color_rows = (DRAW_COLOR_COUNT + 1) / 2;
    by += color_rows * (HUB_CLR_SIZE + HUB_CLR_GAP) - HUB_CLR_GAP + 18;

    // Brush size buttons
    if (ui_button(301, bx, by, 28, 28, (UIColor){0.22f, 0.22f, 0.26f, 0.95f}, (UIColor){0.35f, 0.35f, 0.4f, 1.0f}, false)) {
        if (draw_brush_size > 1) draw_brush_size--;
    }
    // Minus icon manually
    glColor3f(0.95f, 0.95f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(px_to_ndc_x(bx + 8), px_to_ndc_y(by + 14));
        glVertex2f(px_to_ndc_x(bx + 20), px_to_ndc_y(by + 14));
    glEnd();

    if (ui_button(302, bx + 36, by, 28, 28, (UIColor){0.22f, 0.22f, 0.26f, 0.95f}, (UIColor){0.35f, 0.35f, 0.4f, 1.0f}, false)) {
        if (draw_brush_size < 48) draw_brush_size++;
    }
    // Plus icon manually
    glBegin(GL_LINES);
        glVertex2f(px_to_ndc_x(bx + 36 + 8), px_to_ndc_y(by + 14));
        glVertex2f(px_to_ndc_x(bx + 36 + 20), px_to_ndc_y(by + 14));
        glVertex2f(px_to_ndc_x(bx + 36 + 14), px_to_ndc_y(by + 8));
        glVertex2f(px_to_ndc_x(bx + 36 + 14), px_to_ndc_y(by + 20));
    glEnd();
    glLineWidth(1.0f);

    // Brush preview
    {
        int preview = draw_brush_size;
        if (preview > 10) preview = 10;
        UIColor p_color;
        if (draw_use_custom_color) {
            p_color = (UIColor){draw_custom_color[0]/255.0f, draw_custom_color[1]/255.0f, draw_custom_color[2]/255.0f, 1.0f};
        } else {
            const unsigned char* c = draw_palette[draw_color_idx];
            p_color = (UIColor){c[0]/255.0f, c[1]/255.0f, c[2]/255.0f, 1.0f};
        }
        ui_draw_rect(bx + 30 - preview/2, by + 14 - preview/2, preview + 2, preview + 2, p_color);
    }

    by += 28 + 10;

    // Clear button
    if (ui_icon_button(200, bx, by, hub_w - HUB_PAD*2, 34, (UIColor){0.50f, 0.18f, 0.18f, 0.95f}, (UIColor){0.65f, 0.25f, 0.25f, 1.0f}, false, icon_clear_cb, NULL)) {
        draw_clear_layer();
    }

    by += 34 + 10;

    // AI Upscale button
    UIColor ai_color = {0.20f, 0.24f, 0.34f, 0.95f};
    UIColor ai_hover = {0.30f, 0.34f, 0.44f, 1.0f};
    if (ai_processing) ai_color = (UIColor){0.32f, 0.28f, 0.12f, 0.95f};
    else if (ai_failed) ai_color = (UIColor){0.42f, 0.14f, 0.14f, 0.95f};
    else if (ai_ready && ai_show) ai_color = (UIColor){0.14f, 0.38f, 0.30f, 0.95f};

    if (ui_icon_button(400, bx, by, HUB_BTN_SIZE, HUB_BTN_SIZE, ai_color, ai_hover, ai_ready && ai_show, icon_ai_cb, NULL)) {
        ai_upscale_button_activate();
    }

    // Model selection
    int model_btn_w = 28;
    if (ui_icon_button(501, bx, by + HUB_BTN_SIZE + 10, model_btn_w, 28, (UIColor){0.22f, 0.22f, 0.26f, 0.95f}, (UIColor){0.35f, 0.35f, 0.4f, 1.0f}, false, icon_arrow_cb, (void*)(intptr_t)-1)) {
        if (ai_model_count > 0) ai_model_index = (ai_model_index - 1 + ai_model_count) % ai_model_count;
    }
    if (ui_icon_button(502, bx + hub_w - HUB_PAD*2 - model_btn_w, by + HUB_BTN_SIZE + 10, model_btn_w, 28, (UIColor){0.22f, 0.22f, 0.26f, 0.95f}, (UIColor){0.35f, 0.35f, 0.4f, 1.0f}, false, icon_arrow_cb, (void*)(intptr_t)1)) {
        if (ai_model_count > 0) ai_model_index = (ai_model_index + 1) % ai_model_count;
    }

    // Model name and description
    int text_x = bx + model_btn_w + 10;
    int text_y = by + HUB_BTN_SIZE + 10 + 8;
    int left_margin = -38;
    const char* model_name = (ai_model_count > 0 && ai_model_index >=0 && ai_model_index < ai_model_count) ? ai_model_names[ai_model_index] : "No models";
    draw_text(px_to_ndc_x(text_x+left_margin), px_to_ndc_y(text_y + 70), model_name);

    const char* model_desc = (ai_model_count > 0 && ai_model_index >=0 && ai_model_index < ai_model_count) ? ai_model_descriptions[ai_model_index] : "";
    if (model_desc[0] != '\0') {
        glColor3f(0.75f, 0.75f, 0.80f);
        draw_text(px_to_ndc_x(text_x+left_margin), px_to_ndc_y(text_y + 85), model_desc);
        glColor3f(1.0f, 1.0f, 1.0f);
    }

    // Filter mode and AI state at the bottom
    static const char *filter_mode_names[] = {
        "Off", "Smooth", "Sharpen", "Pixelated", "Lanczos", "FSR EASU", "Denoise", "RCAS EASU", "Hybrid", "Edge"  
    };
    char mode_text[64];
    sprintf(mode_text, "Mode: %s", filter_mode_names[filter_mode]);
    draw_text(px_to_ndc_x(bx), px_to_ndc_y(hub_y + hub_h + 30), mode_text);

    char ai_text[128];
    const char *ai_state = "not processed";
    if (ai_processing) ai_state = "processing";
    else if (ai_failed) ai_state = "failed";
    else if (ai_ready && ai_show) ai_state = "AI";
    else if (ai_ready) ai_state = "Original";

    if (ai_ready && ai_processing_time_ms > 0)
        sprintf(ai_text, "AI: %s (%.2f sec)", ai_state, ai_processing_time_ms / 1000.0f);
    else
        sprintf(ai_text, "AI: %s", ai_state);
        
    draw_text(px_to_ndc_x(bx), px_to_ndc_y(hub_y + hub_h + 45), ai_text);

    ui_end();
}

static void render_help_panel_bg(void)
{
    int px = (screen_w - HELP_PANEL_W) / 2;
    int py = (screen_h - HELP_PANEL_H) / 2;

    if (px < 8)
        px = 8;
    if (py < 8)
        py = 8;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ui_draw_rect(px, py, HELP_PANEL_W, HELP_PANEL_H, (UIColor){0.07f, 0.07f, 0.09f, 0.94f});
    ui_draw_rect_outline(px, py, HELP_PANEL_W, HELP_PANEL_H, (UIColor){0.55f, 0.55f, 0.62f, 1.0f}, 1.0f);

    glDisable(GL_BLEND);
}

static inline void help_overlay_destroy_font(void)
{
    if (help_overlay_font) {
        DeleteObject(help_overlay_font);
        help_overlay_font = NULL;
    }
}

static int init_font(void)
{
    // Try to load Segoe UI from Windows system directory
    char font_path[MAX_PATH];
    GetWindowsDirectoryA(font_path, MAX_PATH);
    strcat_s(font_path, MAX_PATH, "\\Fonts\\segoeui.ttf");

    FILE* f = NULL;
    if (fopen_s(&f, font_path, "rb") != 0 || !f) {
        // Fallback to Arial if Segoe UI not found
        GetWindowsDirectoryA(font_path, MAX_PATH);
        strcat_s(font_path, MAX_PATH, "\\Fonts\\arial.ttf");
        if (fopen_s(&f, font_path, "rb") != 0 || !f) {
            return 0;
        }
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    font_data = (unsigned char*)malloc(size);
    if (!font_data) {
        fclose(f);
        return 0;
    }
    fread(font_data, 1, size, f);
    fclose(f);

    // Bake font to texture
    unsigned char* temp_bitmap = (unsigned char*)malloc(font_texture_width * font_texture_height);
    if (!temp_bitmap) {
        free(font_data);
        return 0;
    }
    // y_is_top_down = 1 because we use top-left origin
    stbtt_BakeFontBitmap(font_data, 0, 18.0f, temp_bitmap, font_texture_width, font_texture_height, 32, 96, cdata);

    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, font_texture_width, font_texture_height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, temp_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(temp_bitmap);
    return 1;
}

static void draw_text(float ndc_x, float ndc_y, const char* text)
{
    if (!font_texture) return;

    // Convert NDC start position to pixels
    float px_x = (ndc_x + 1.0f) / 2.0f * screen_w;
    float px_y = (1.0f - ndc_y) / 2.0f * screen_h;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glColor3f(0.91f, 0.92f, 0.94f); // Light gray color (matches original)

    for (int i = 0; text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 32 || c >= 128) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(cdata, font_texture_width, font_texture_height, c - 32, &px_x, &px_y, &q, 1); // y_is_top_down = 1

        // Convert pixel coordinates to NDC
        float x0 = (q.x0 / screen_w) * 2.0f - 1.0f;
        float x1 = (q.x1 / screen_w) * 2.0f - 1.0f;
        float y0 = 1.0f - (q.y0 / screen_h) * 2.0f;
        float y1 = 1.0f - (q.y1 / screen_h) * 2.0f;

        glBegin(GL_QUADS);
            glTexCoord2f(q.s0, q.t0); glVertex2f(x0, y0);
            glTexCoord2f(q.s1, q.t0); glVertex2f(x1, y0);
            glTexCoord2f(q.s1, q.t1); glVertex2f(x1, y1);
            glTexCoord2f(q.s0, q.t1); glVertex2f(x0, y1);
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void cleanup_font(void)
{
    if (font_data) {
        free(font_data);
        font_data = NULL;
    }
    if (font_texture) {
        glDeleteTextures(1, &font_texture);
        font_texture = 0;
    }
}

static void render_help_overlay_text(void)
{
    int px, py, i;
    static const char *lines[] = {
        "Zomer — Screenshot Tool",
        "",
        "Win+Z              Open window",
        "Ctrl+Win+Z         Copy to clipboard (quiet)",
        "Esc/F2                Close window",
        "`                  Show this help",
        "R                  Reset zoom and position",
        "F                  Cycle filters / F+0-8 for specific filter",
        "F+0..8             Filter (0=Off 1=Smooth 2=Sharp 3=Pixel 4=Lanczos 5=FSR 6=Denoise 7=RCAS 8=Hybrid)",
        "Ctrl + Scroll      Zoom to cursor",
        "RMB + Drag         Move image",
        "X / Z              Flip X / Y",
        "",
        "C                  Save screenshot + drawing",
        "Shift+C            Save current view",
        "Ctrl+C / P         Copy current view to clipboard",
        "",
        "T                  Text mode",
        "B / N              Brightness + / -",
        "V / M              Contrast + / -",
        "G / H              Gamma + / -",
        "A                  Toggle borderless/normal window",
        "",
        "Tab                Drawing panel",
        "Tab panel AI       Run AI upscale once, then toggle Original / AI",
        "U                  Run AI upscale",
        "I                  Save AI-upscaled image + drawing",
        "LMB                Draw (when Tab active)",
        "[ / ]              Pen size / line width",
        "Ctrl+Z             Undo drawing",
        "D + Scroll         Flashlight / size",
        "Q                  Select area (copy to clipboard)",
        "Shift+Q            Select area (save to file)",
        NULL
    };

    if (!help_overlay_visible || !window)
        return;

    // Initialize font if not done yet
    if (!font_texture) {
        if (!init_font()) {
            return;
        }
    }

    px = (screen_w - HELP_PANEL_W) / 2;
    py = (screen_h - HELP_PANEL_H) / 2;
    if (px < 8) px = 8;
    if (py < 8) py = 8;

    // Convert pixel coordinates to OpenGL NDC for starting position
    float start_x_ndc = px_to_ndc_x(px + 22);
    float start_y_ndc = px_to_ndc_y(py + 18);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    float current_y = start_y_ndc;
    float line_height_ndc = (float)HELP_LINE_H / (float)screen_h * 2.0f;

    for (i = 0; lines[i]; i++) {
        if (lines[i][0]) {
            draw_text(start_x_ndc, current_y, lines[i]);
        }
        current_y -= line_height_ndc;
    }
}

static void render_help_overlay(void)
{
    if (!help_overlay_visible)
        return;

    render_help_panel_bg();
    render_help_overlay_text();
}

static void render_shape_preview(void)
{
    const unsigned char *color;
    float r, g, b, a;

    if (!draw_hub_visible || !draw_stroke_active || !draw_lmb_was)
        return;

    if (draw_tool != DRAW_TOOL_LINE && draw_tool != DRAW_TOOL_RECT && draw_tool != DRAW_TOOL_ELLIPSE)
        return;

    if (draw_use_custom_color) {
        color = draw_custom_color;
    } else {
        color = draw_palette[draw_color_idx];
    }
    r = color[0] / 255.0f;
    g = color[1] / 255.0f;
    b = color[2] / 255.0f;
    a = 0.95f;

    glMatrixMode(GL_MODELVIEW);
    apply_content_view_transform();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(r, g, b, a);
    {
        float lw = (float)draw_tool_brush_radius() * 0.85f;
        if (lw < 1.0f) lw = 1.0f;
        glLineWidth(lw);
    }

    glBegin(GL_LINES);
    if (draw_tool == DRAW_TOOL_LINE) {
        glVertex2f(content_px_to_ndc_x(draw_start_px), content_px_to_ndc_y(draw_start_py));
        glVertex2f(content_px_to_ndc_x(draw_cur_px), content_px_to_ndc_y(draw_cur_py));
    } else if (draw_tool == DRAW_TOOL_RECT) {
        int x0 = draw_start_px, y0 = draw_start_py;
        int x1 = draw_cur_px, y1 = draw_cur_py;
        glVertex2f(content_px_to_ndc_x(x0), content_px_to_ndc_y(y0));
        glVertex2f(content_px_to_ndc_x(x1), content_px_to_ndc_y(y0));
        glVertex2f(content_px_to_ndc_x(x1), content_px_to_ndc_y(y0));
        glVertex2f(content_px_to_ndc_x(x1), content_px_to_ndc_y(y1));
        glVertex2f(content_px_to_ndc_x(x1), content_px_to_ndc_y(y1));
        glVertex2f(content_px_to_ndc_x(x0), content_px_to_ndc_y(y1));
        glVertex2f(content_px_to_ndc_x(x0), content_px_to_ndc_y(y1));
        glVertex2f(content_px_to_ndc_x(x0), content_px_to_ndc_y(y0));
    }
    glEnd();

    if (draw_tool == DRAW_TOOL_ELLIPSE) {
        int cx = (draw_start_px + draw_cur_px) / 2;
        int cy = (draw_start_py + draw_cur_py) / 2;
        int rx = abs(draw_cur_px - draw_start_px) / 2;
        int ry = abs(draw_cur_py - draw_start_py) / 2;
        int s;

        if (rx < 1) rx = 1;
        if (ry < 1) ry = 1;

        glBegin(GL_LINE_LOOP);
        for (s = 0; s < 48; s++) {
            float ang = s * 6.2831853f / 48.0f;
            glVertex2f(content_px_to_ndc_x(cx + (int)(rx * cosf(ang))),
                       content_px_to_ndc_y(cy + (int)(ry * sinf(ang))));
        }
        glEnd();
    }

    glLineWidth(1.0f);
    glDisable(GL_BLEND);
}

static void process_draw_input(GLFWwindow *win)
{
    double mx, my;
    int screen_px, screen_py;
    int px, py;
    int lmb;
    int hit;
    const unsigned char *color;
    int brush;
    int erase;

    if (!draw_pixels || !draw_hub_visible)
        return;

    glfwGetCursorPos(win, &mx, &my);
    screen_px = (int)mx;
    screen_py = (int)my;
    screen_px_to_content(screen_px, screen_py, &px, &py);
    draw_cur_px = px;
    draw_cur_py = py;
    lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    if (ui_is_hovering()) {
        draw_lmb_was = lmb;
        draw_stroke_active = 0;
        return;
    }

    if (is_dragging) {
        draw_lmb_was = lmb;
        draw_stroke_active = 0;
        return;
    }

    if (draw_use_custom_color) {
        color = draw_custom_color;
    } else {
        color = draw_palette[draw_color_idx];
    }
    erase = (draw_tool == DRAW_TOOL_ERASER);
    brush = draw_tool_brush_radius();

    if (lmb) {
        if (!draw_lmb_was) {
            if (draw_tool == DRAW_TOOL_PIPETTE) {
                // Pipette: pick color from screenshot
                if (px >= 0 && px < screen_width && py >= 0 && py < screen_height) {
                    size_t idx = ((size_t)py * screen_width + (size_t)px) * 4;
                    unsigned char r = pixeldata_ptr[idx + 0];
                    unsigned char g = pixeldata_ptr[idx + 1];
                    unsigned char b = pixeldata_ptr[idx + 2];
                    draw_custom_color[0] = r;
                    draw_custom_color[1] = g;
                    draw_custom_color[2] = b;
                    draw_custom_color[3] = 255;
                    draw_use_custom_color = 1;
                }
                draw_lmb_was = lmb;
                return;
            }
            draw_undo_push();
            draw_start_px = px;
            draw_start_py = py;
            draw_last_px = px;
            draw_last_py = py;
            draw_stroke_active = 1;

            if (draw_tool == DRAW_TOOL_PENCIL || draw_tool == DRAW_TOOL_ERASER) {
                draw_brush(px, py, brush, color, erase);
            } else if (draw_tool == DRAW_TOOL_BUCKET) {
                draw_flood_fill(px, py, color);
                draw_stroke_active = 0;
            }
        } else if (draw_stroke_active) {
            if (draw_tool == DRAW_TOOL_PENCIL || draw_tool == DRAW_TOOL_ERASER) {
                draw_line_pixels(draw_last_px, draw_last_py, px, py, brush, color, erase);
                draw_last_px = px;
                draw_last_py = py;
            }
        }
    } else if (draw_lmb_was && draw_stroke_active) {
        if (draw_tool == DRAW_TOOL_LINE) {
            draw_line_pixels(draw_start_px, draw_start_py, draw_cur_px, draw_cur_py, brush, color, 0);
        } else if (draw_tool == DRAW_TOOL_RECT) {
            draw_rect_outline(draw_start_px, draw_start_py, draw_cur_px, draw_cur_py, brush, color, 0);
        } else if (draw_tool == DRAW_TOOL_ELLIPSE) {
            int cx = (draw_start_px + draw_cur_px) / 2;
            int cy = (draw_start_py + draw_cur_py) / 2;
            int rx = abs(draw_cur_px - draw_start_px) / 2;
            int ry = abs(draw_cur_py - draw_start_py) / 2;
            draw_ellipse_outline(cx, cy, rx, ry, brush, color, 0);
        }
        draw_stroke_active = 0;
    }

    if (!lmb)
        draw_stroke_active = 0;

    draw_lmb_was = lmb;
}

static inline void reset_view_state(void)
{
    zoom_level = 1.0f;
    offset_x = 0.0f;
    offset_y = 0.0f;
    flip_x = 1;
    flip_y = 1;
    is_dragging = 0;
    flashlight_mode = 0;
    flash_locked = 0;
    flashlight_radius = 260.0f;
    text_mode = 0;
    image_contrast = 1.0f;
    image_brightness = 0.0f;
    image_gamma = 1.0f;
}

static inline void destroy_capture_window(void)
{
    if (!window)
        return;

    if (textureID) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
    }

    ai_upscale_clear();
    upscale_cleanup(&upscale_pipeline);

    destroy_draw_layer();
    help_overlay_destroy_font();
    cleanup_font();
    help_overlay_visible = 0;

    // Очистка асинхронного збереження
    if (save_thread_handle) {
        WaitForSingleObject(save_thread_handle, INFINITE);
        CloseHandle(save_thread_handle);
        save_thread_handle = NULL;
    }
    free(save_pixels);
    save_pixels = NULL;
    save_width = 0;
    save_height = 0;
    InterlockedExchange(&save_thread_done, 0);

    free(pixeldata_ptr);
    pixeldata_ptr = NULL;

    glfwDestroyWindow(window);
    window = NULL;
    window_visible = 0;
    focus_frames_remaining = 0;

    if (glfw_active) {
        glfwTerminate();
        glfw_active = 0;
    }
}

static int create_capture_window(void)
{
    const char *description;
    HWND hwnd;

    if (window)
        return 1;

    screen_width = GetSystemMetrics(SM_CXSCREEN);
    screen_height = GetSystemMetrics(SM_CYSCREEN);
    screen_w = screen_width;
    screen_h = screen_height;

    reset_view_state();

    if (!glfwInit()) {
        glfwGetError(&description);
        printf("GLFW error: %s\n", description);
        return 0;
    }
    glfw_active = 1;

    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

    window = glfwCreateWindow(screen_width, screen_height, "Zomer", NULL, NULL);
    if (!window) {
        glfwTerminate();
        glfw_active = 0;
        return 0;
    }

    hwnd = glfwGetWin32Window(window);
    glfwMakeContextCurrent(window);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetWindowSizeCallback(window, window_size_callback);

    window_width = screen_width;
    window_height = screen_height;
    screen_w = screen_width;
    screen_h = screen_height;
    original_window_width = screen_width;
    original_window_height = screen_height;

    if (!gladLoadGL()) {
        destroy_capture_window();
        printf("Failed to initialize GLAD\n");
        return 0;
    }

    glfwSwapInterval(1);

    pixeldata_ptr = (unsigned char*)malloc((size_t)screen_width * (size_t)screen_height * 4);
    if (!pixeldata_ptr) {
        destroy_capture_window();
        printf("Failed to allocate screenshot buffer\n");
        return 0;
    }

    if (!GetScreenShot(pixeldata_ptr, screen_width, screen_height)) {
        pixeldata_ptr = NULL;
        destroy_capture_window();
        printf("Failed to capture screen\n");
        return 0;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixeldata_ptr);

    if (!upscale_init_pipeline(&upscale_pipeline, screen_width, screen_height)) {
        destroy_capture_window();
        printf("Failed to initialize render pipeline\n");
        return 0;
    }

    if (!init_draw_layer()) {
        destroy_capture_window();
        printf("Failed to initialize draw layer\n");
        return 0;
    }

    ui_init();
    init_font();

    apply_filter_mode();

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, screen_width, screen_height, SWP_SHOWWINDOW);
    glfwShowWindow(window);
    focus_capture_window();
    focus_frames_remaining = 120;

    window_visible = 1;
    return 1;
}

static void render_capture_window(void)
{
    int save_visible_requested = 0;
    int copy_visible_requested = 0;

    if (!window)
        return;

    // Перевірка результату AI-апскейлу
    ai_upscale_poll_result();

    if (focus_frames_remaining > 0) {
        focus_capture_window();
        focus_frames_remaining--;
    }

    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        destroy_capture_window();
        return;
    }
    
    if (glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS) {
        destroy_capture_window();
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
        reset_view_state();

    static int f_released = 1;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (f_released) {
            filter_mode = (filter_mode + 1) % FILTER_COUNT;
            apply_filter_mode();
            f_released = 0;
        }
    } else {
        f_released = 1;
    }

    static int c_released = 1;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (c_released) {
            int ctrl_pressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
                               (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
            int shift_pressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                                (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

            if (ctrl_pressed)
                copy_visible_requested = 1;
            else if (shift_pressed)
                save_visible_requested = 1;
            else
                save_screenshot_to_desktop();

            c_released = 0;
        }
    } else {
        c_released = 1;
    }

    static int p_released = 1;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
        if (p_released) {
            copy_visible_requested = 1;
            p_released = 0;
        }
    } else {
        p_released = 1;
    }

    static int i_released = 1;
    if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
        if (i_released) {
            save_ai_upscaled_with_drawing_to_desktop();
            i_released = 0;
        }
    } else {
        i_released = 1;
    }

    static int u_released = 1;
    if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) {
        if (u_released) {
            ai_upscale_button_activate();
            u_released = 0;
        }
    } else {
        u_released = 1;
    }

    static int minus_released = 1;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
        if (minus_released) {
            if (ai_model_count > 0) {
                ai_model_index = (ai_model_index - 1 + ai_model_count) % ai_model_count;
            }
            minus_released = 0;
        }
    } else {
        minus_released = 1;
    }

    static int plus_released = 1;
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
        if (plus_released) {
            if (ai_model_count > 0) {
                ai_model_index = (ai_model_index + 1) % ai_model_count;
            }
            plus_released = 0;
        }
    } else {
        plus_released = 1;
    }

    static int t_released = 1;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (t_released) {
            text_mode = !text_mode;
            t_released = 0;
        }
    } else {
        t_released = 1;
    }

    static int f_pressed = 0;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        if (!f_pressed) {
            f_pressed = 1;
        }
    } else {
        f_pressed = 0;
    }

    static int num_released[9] = {1,1,1,1,1,1,1,1,1};
    int num_keys[10] = {GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8};
    if (f_pressed) {
        for (int i = 0; i < 9; i++) {
            if (glfwGetKey(window, num_keys[i]) == GLFW_PRESS) {
                if (num_released[i]) {
                    filter_mode = i;
                    num_released[i] = 0;
                }
            } else {
                num_released[i] = 1;
            }
        }
    }

    static int tab_released = 1;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (tab_released) {
            draw_hub_visible = !draw_hub_visible;
            tab_released = 0;
        }
    } else {
        tab_released = 1;
    }

    static int a_released = 1;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        if (a_released) {
            window_bordered = !window_bordered;
            HWND hwnd = glfwGetWin32Window(window);
            if (window_bordered) {
                // Звичайне вікно з рамкою (не TOPMOST)
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
                // Визначаємо розмір вікна з урахуванням рамок
                RECT rect = { 0, 0, screen_width, screen_height };
                AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
                int w = rect.right - rect.left;
                int h = rect.bottom - rect.top;
                // Встановлюємо розмір та позицію по центру
                int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
                int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;
                SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, w, h, SWP_SHOWWINDOW);
                glfwSetWindowSize(window, w, h);
                // Важливо: screen_w/screen_h ЗАВЖДИ = розмір скріншоту, щоб функції перетворення працювали!
                window_width = w;
                window_height = h;
                screen_w = screen_width;
                screen_h = screen_height;
            } else {
                // Безрамкове вікно на весь екран (TOPMOST)
                glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
                glfwSetWindowSize(window, screen_width, screen_height);
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, screen_width, screen_height, SWP_SHOWWINDOW);
                // Важливо: screen_w/screen_h ЗАВЖДИ = розмір скріншоту, щоб функції перетворення працювали!
                window_width = screen_width;
                window_height = screen_height;
                screen_w = screen_width;
                screen_h = screen_height;
            }
            a_released = 0;
        }
    } else {
        a_released = 1;
    }

    static int grave_released = 1;
    if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS) {
        if (grave_released) {
            help_overlay_visible = !help_overlay_visible;
            grave_released = 0;
        }
    } else {
        grave_released = 1;
    }

    static int b_released = 1;
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS) {
        if (b_released) {
            image_brightness += 0.04f;
            if (image_brightness > 0.40f) image_brightness = 0.40f;
            b_released = 0;
        }
    } else {
        b_released = 1;
    }

    static int n_released = 1;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
        if (n_released) {
            image_brightness -= 0.04f;
            if (image_brightness < -0.40f) image_brightness = -0.40f;
            n_released = 0;
        }
    } else {
        n_released = 1;
    }

    static int v_released = 1;
    if (glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS) {
        if (v_released) {
            image_contrast += 0.08f;
            if (image_contrast > 2.20f) image_contrast = 2.20f;
            v_released = 0;
        }
    } else {
        v_released = 1;
    }

    static int m_released = 1;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS) {
        if (m_released) {
            image_contrast -= 0.08f;
            if (image_contrast < 0.40f) image_contrast = 0.40f;
            m_released = 0;
        }
    } else {
        m_released = 1;
    }

    static int g_released = 1;
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS) {
        if (g_released) {
            image_gamma += 0.08f;
            if (image_gamma > 2.40f) image_gamma = 2.40f;
            g_released = 0;
        }
    } else {
        g_released = 1;
    }

    static int h_released = 1;
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS) {
        if (h_released) {
            image_gamma -= 0.08f;
            if (image_gamma < 0.40f) image_gamma = 0.40f;
            h_released = 0;
        }
    } else {
        h_released = 1;
    }

    static int z_released = 1;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        if (z_released) {
            int ctrl_pressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) ||
                               (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

            if (ctrl_pressed) {
                if (draw_undo_count > 0)
                    draw_undo_pop();
            } else {
                flip_y *= -1;
            }

            z_released = 0;
        }
    } else {
        z_released = 1;
    }

    if (draw_hub_visible) {
        static int bracket_l_released = 1;
        static int bracket_r_released = 1;

        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS) {
            if (bracket_l_released && draw_brush_size > 1)
                draw_brush_size--;
            bracket_l_released = 0;
        } else {
            bracket_l_released = 1;
        }

        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS) {
            if (bracket_r_released && draw_brush_size < 48)
                draw_brush_size++;
            bracket_r_released = 0;
        } else {
            bracket_r_released = 1;
        }
    }

    static int x_released = 1;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        if (x_released) {
            flip_x *= -1;
            x_released = 0;
        }
    } else {
        x_released = 1;
    }

    static int q_released = 1;
    int q_pressed_now = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
    int shift_pressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) || (glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

    if (q_pressed_now) {
        if (!selection_active && q_released) {
            // Start selection
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int px, py;
            screen_px_to_content((int)mx, (int)my, &px, &py);
            selection_start_x = px;
            selection_start_y = py;
            selection_end_x = px;
            selection_end_y = py;
            selection_active = 1;
            q_pressed = 1;
        } else if (selection_active) {
            // Update selection
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int px, py;
            screen_px_to_content((int)mx, (int)my, &px, &py);
            selection_end_x = px;
            selection_end_y = py;
        }
        q_released = 0;
    } else {
        if (q_pressed && selection_active) {
            // Finish selection - save or copy
            if (shift_pressed) {
                save_selection_to_desktop(selection_start_x, selection_start_y, selection_end_x, selection_end_y);
            } else {
                copy_selection_to_clipboard(selection_start_x, selection_start_y, selection_end_x, selection_end_y);
            }
            selection_active = 0;
            q_pressed = 0;
        }
        q_released = 1;
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        float mouse_gl_x = (float)((mouse_x / screen_w) * 2.0 - 1.0);
        float mouse_gl_y = (float)(1.0 - (mouse_y / screen_h) * 2.0);

        flash_target_x = mouse_gl_x;
        flash_target_y = mouse_gl_y;
        flashlight_mode = 1;
    } else {
        flashlight_mode = 0;
    }

    process_draw_input(window);

    {
        GLuint active_texture = (ai_ready && ai_show && ai_texture_id) ? ai_texture_id : textureID;
        int active_image_width = (ai_ready && ai_show && ai_texture_id) ? ai_width : screen_width;
        int active_image_height = (ai_ready && ai_show && ai_texture_id) ? ai_height : screen_height;

        if (upscale_pipeline.fboID && upscale_pipeline.fboTexID && upscale_pipeline.rcasProgram) {
            upscale_render_pass(&upscale_pipeline, active_texture, filter_mode, zoom_level, offset_x, offset_y,
                                screen_width, screen_height, flip_x, flip_y, active_image_width, active_image_height);
            upscale_rcas_pass(&upscale_pipeline, filter_mode, image_contrast, image_brightness, image_gamma, screen_width, screen_height, 0.45f, text_mode, zoom_level);
        }
    }

    if (draw_dirty)
        upload_draw_texture();

    render_draw_layer();
    render_shape_preview();
    render_selection_rect();

    if (flashlight_mode) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        draw_circular_flashlight(flash_target_x, flash_target_y);
    }

    render_and_handle_ui(window);

    render_help_overlay();

    if (save_visible_requested)
        save_visible_frame_to_desktop();

    if (copy_visible_requested)
        copy_visible_frame_to_clipboard();

    glfwSwapBuffers(window);

    if (glfwWindowShouldClose(window))
        destroy_capture_window();
}

LRESULT CALLBACK hotkey_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_HOTKEY) {
        if (wparam == HOTKEY_ID_WIN_Z || wparam == HOTKEY_ID_WIN_SHIFT_Z) {
            create_capture_window();
        } else if (wparam == HOTKEY_ID_WIN_CTRL_Z) {
            // Quiet mode: copy to clipboard
            copy_visible_frame_to_clipboard();
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    static int win_z_down = 0;

    if (code == HC_ACTION) {
        KBDLLHOOKSTRUCT *key = (KBDLLHOOKSTRUCT*)lparam;
        int key_down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
        int key_up = (wparam == WM_KEYUP || wparam == WM_SYSKEYUP);

        if (key->vkCode == 'Z') {
            int win_pressed = (GetAsyncKeyState(VK_LWIN) & 0x8000) || (GetAsyncKeyState(VK_RWIN) & 0x8000);
            int shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
            int ctrl_pressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

            if (key_down && win_pressed && !win_z_down) {
                win_z_down = 1;
                if (ctrl_pressed) {
                    PostMessage(hotkey_hwnd, WM_HOTKEY, HOTKEY_ID_WIN_CTRL_Z, 0);
                } else if (shift_pressed) {
                    PostMessage(hotkey_hwnd, WM_HOTKEY, HOTKEY_ID_WIN_SHIFT_Z, 0);
                } else {
                    PostMessage(hotkey_hwnd, WM_HOTKEY, HOTKEY_ID_WIN_Z, 0);
                }
                return 1;
            }

            if (key_up)
                win_z_down = 0;
        }
    }

    return CallNextHookEx(keyboard_hook, code, wparam, lparam);
}

static HWND create_hotkey_window(HINSTANCE instance)
{
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = hotkey_wndproc;
    wc.hInstance = instance;
    wc.lpszClassName = "ZomerHotkeyWindow";

    RegisterClassA(&wc);

    return CreateWindowExA(
        0,
        wc.lpszClassName,
        "ZomerHotkeyWindow",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE,
        NULL,
        instance,
        NULL
    );
}

int main(void)
{
    MSG msg;
    HINSTANCE instance = GetModuleHandle(NULL);

    scan_models();

    hotkey_hwnd = create_hotkey_window(instance);
    if (!hotkey_hwnd) {
        printf("Failed to create hotkey window\n");
        return -1;
    }

    // Try to register all hotkeys
    int registered = 0;
    registered += RegisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_Z, MOD_WIN | MOD_NOREPEAT, 'Z');
    registered += RegisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_SHIFT_Z, MOD_WIN | MOD_SHIFT | MOD_NOREPEAT, 'Z');
    registered += RegisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_CTRL_Z, MOD_WIN | MOD_CONTROL | MOD_NOREPEAT, 'Z');

    if (registered < 3) {
        // Fall back to keyboard hook if any hotkey failed to register
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_Z);
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_SHIFT_Z);
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_CTRL_Z);
        keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboard_hook_proc, instance, 0);
        using_keyboard_hook = keyboard_hook != NULL;

        if (!using_keyboard_hook) {
            DestroyWindow(hotkey_hwnd);
            printf("Failed to register Win+Z hotkey\n");
            return -1;
        }
    }

    for (;;) {
        if (window) {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    goto shutdown;

                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            render_capture_window();
            Sleep(1);
        } else {
            if (GetMessage(&msg, NULL, 0, 0) <= 0)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

shutdown:
    destroy_capture_window();

    if (using_keyboard_hook)
        UnhookWindowsHookEx(keyboard_hook);
    else {
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_Z);
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_SHIFT_Z);
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID_WIN_CTRL_Z);
    }

    DestroyWindow(hotkey_hwnd);

    return 0;
}
