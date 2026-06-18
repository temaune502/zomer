#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "glfw\include\glad\glad.h"
#define GLFW_INCLUDE_NONE
#include "glfw\include\GLFW\glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw\include\GLFW\glfw3native.h"

#define UTILS_IMPLEMENTATION
#include "utils.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "glfw\stb_image_write.h"

#define HOTKEY_ID 1

enum {
    FILTER_OFF = 0,
    FILTER_SMOOTH = 1,
    FILTER_SHARPEN = 2,
    FILTER_PIXELATED = 3,
    FILTER_LANCZOS = 4,
    FILTER_COUNT = 5
};

float zoom_level = 1.0f;
float offset_x = 0.0f;
float offset_y = 0.0f;
float flashlight_radius = 260.0f;

int window_visible = 0;
unsigned int textureID = 0;
unsigned int filterProgram = 0;
int filterTexelSizeLoc = -1;
int filterSharpnessLoc = -1;
int filterModeLoc = -1;
int filterContrastLoc = -1;
int filterBrightnessLoc = -1;
int filterGammaLoc = -1;
int filterTextModeLoc = -1;
int filterZoomLoc = -1;
unsigned char* pixeldata_ptr = NULL;
int screen_width = 0;
int screen_height = 0;
int filter_mode = FILTER_SHARPEN;
int text_mode = 0;
float image_contrast = 1.0f;
float image_brightness = 0.0f;
float image_gamma = 1.0f;

HWND hotkey_hwnd = NULL;
HHOOK keyboard_hook = NULL;
int using_keyboard_hook = 0;

static GLFWwindow *window = NULL;
static int glfw_active = 0;
static int focus_frames_remaining = 0;

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

static inline void focus_capture_window(void)
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

static inline unsigned int compile_shader(unsigned int type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    int ok = 0;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static inline int create_filter_program(void)
{
    const char *vertex_source =
        "#version 120\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    vTexCoord = gl_MultiTexCoord0.xy;\n"
        "    gl_Position = ftransform();\n"
        "}\n";

    const char *fragment_source =
        "#version 120\n"
        "uniform sampler2D screenTex;\n"
        "uniform vec2 texelSize;\n"
        "uniform float sharpness;\n"
        "uniform int filterMode;\n"
        "uniform float contrast;\n"
        "uniform float brightness;\n"
        "uniform float gammaValue;\n"
        "uniform int textMode;\n"
        "uniform float zoomLevel;\n"
        "varying vec2 vTexCoord;\n"
        "void main() {\n"
        "    vec4 center = texture2D(screenTex, vTexCoord);\n"
        "    vec4 left = texture2D(screenTex, vTexCoord + vec2(-texelSize.x, 0.0));\n"
        "    vec4 right = texture2D(screenTex, vTexCoord + vec2(texelSize.x, 0.0));\n"
        "    vec4 up = texture2D(screenTex, vTexCoord + vec2(0.0, -texelSize.y));\n"
        "    vec4 down = texture2D(screenTex, vTexCoord + vec2(0.0, texelSize.y));\n"
        "    vec4 ul = texture2D(screenTex, vTexCoord + vec2(-texelSize.x, -texelSize.y));\n"
        "    vec4 ur = texture2D(screenTex, vTexCoord + vec2(texelSize.x, -texelSize.y));\n"
        "    vec4 dl = texture2D(screenTex, vTexCoord + vec2(-texelSize.x, texelSize.y));\n"
        "    vec4 dr = texture2D(screenTex, vTexCoord + vec2(texelSize.x, texelSize.y));\n"
        "    vec4 blur = (left + right + up + down) * 0.25;\n"
        "    vec4 color = center;\n"
        "    float zoomSharpness = zoomLevel > 1.2 ? sharpness : sharpness * 0.35;\n"
        "    if (filterMode == 1) {\n"
        "        color = center * 0.68 + blur * 0.32;\n"
        "    } else if (filterMode == 2) {\n"
        "        color = center + (center - blur) * zoomSharpness;\n"
        "    } else if (filterMode == 4) {\n"
        "        vec4 wideBlur = (left + right + up + down) * 0.18 + (ul + ur + dl + dr) * 0.07;\n"
        "        color = center + (center - wideBlur) * (zoomSharpness * 0.85);\n"
        "    }\n"
        "    if (textMode == 1) {\n"
        "        float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"
        "        vec3 gray = vec3(luma);\n"
        "        color.rgb = mix(color.rgb, gray, 0.28);\n"
        "        color.rgb = (color.rgb - 0.5) * 1.28 + 0.5;\n"
        "        color = color + (color - blur) * 0.35;\n"
        "    }\n"
        "    color.rgb = (color.rgb - 0.5) * contrast + 0.5 + brightness;\n"
        "    color.rgb = pow(clamp(color.rgb, 0.0, 1.0), vec3(1.0 / gammaValue));\n"
        "    gl_FragColor = vec4(clamp(color.rgb, 0.0, 1.0), center.a);\n"
        "}\n";

    unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    unsigned int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    int ok = 0;

    if (!vertex_shader || !fragment_shader) {
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        return 0;
    }

    filterProgram = glCreateProgram();
    glAttachShader(filterProgram, vertex_shader);
    glAttachShader(filterProgram, fragment_shader);
    glLinkProgram(filterProgram);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGetProgramiv(filterProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        glDeleteProgram(filterProgram);
        filterProgram = 0;
        return 0;
    }

    glUseProgram(filterProgram);
    glUniform1i(glGetUniformLocation(filterProgram, "screenTex"), 0);
    filterTexelSizeLoc = glGetUniformLocation(filterProgram, "texelSize");
    filterSharpnessLoc = glGetUniformLocation(filterProgram, "sharpness");
    filterModeLoc = glGetUniformLocation(filterProgram, "filterMode");
    filterContrastLoc = glGetUniformLocation(filterProgram, "contrast");
    filterBrightnessLoc = glGetUniformLocation(filterProgram, "brightness");
    filterGammaLoc = glGetUniformLocation(filterProgram, "gammaValue");
    filterTextModeLoc = glGetUniformLocation(filterProgram, "textMode");
    filterZoomLoc = glGetUniformLocation(filterProgram, "zoomLevel");
    glUseProgram(0);

    return 1;
}

static inline void apply_filter_mode(void)
{
    int texture_filter = (filter_mode == FILTER_PIXELATED) ? GL_NEAREST : GL_LINEAR;

    if (!textureID)
        return;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);
}

static inline void save_screenshot_to_desktop(void)
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

    stbi_write_png(path, screen_width, screen_height, 4, pixeldata_ptr, screen_width * 4);
}

static inline void save_visible_frame_to_desktop(void)
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

static inline int copy_visible_frame_to_clipboard(void)
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

static inline int image_adjustments_active(void)
{
    return text_mode ||
           image_contrast != 1.0f ||
           image_brightness != 0.0f ||
           image_gamma != 1.0f;
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

    if (filterProgram) {
        glDeleteProgram(filterProgram);
        filterProgram = 0;
        filterTexelSizeLoc = -1;
        filterSharpnessLoc = -1;
        filterModeLoc = -1;
        filterContrastLoc = -1;
        filterBrightnessLoc = -1;
        filterGammaLoc = -1;
        filterTextModeLoc = -1;
        filterZoomLoc = -1;
    }

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

static inline int create_capture_window(void)
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

    if (!gladLoadGL()) {
        destroy_capture_window();
        printf("Failed to initialize GLAD\n");
        return 0;
    }

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

    create_filter_program();
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

    if (focus_frames_remaining > 0) {
        focus_capture_window();
        focus_frames_remaining--;
    }

    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
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

    static int t_released = 1;
    if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS) {
        if (t_released) {
            text_mode = !text_mode;
            t_released = 0;
        }
    } else {
        t_released = 1;
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
            flip_y *= -1;
            z_released = 0;
        }
    } else {
        z_released = 1;
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

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glTranslatef(offset_x, offset_y, 0.0f);
    glScalef(zoom_level * flip_x, zoom_level * flip_y, 1.0f);

    glEnable(GL_TEXTURE_2D);
    if (filterProgram && ((filter_mode != FILTER_OFF && filter_mode != FILTER_PIXELATED) || image_adjustments_active())) {
        glUseProgram(filterProgram);
        glUniform2f(filterTexelSizeLoc, 1.0f / (float)screen_width, 1.0f / (float)screen_height);
        glUniform1f(filterSharpnessLoc, 0.45f);
        glUniform1i(filterModeLoc, filter_mode);
        glUniform1f(filterContrastLoc, image_contrast);
        glUniform1f(filterBrightnessLoc, image_brightness);
        glUniform1f(filterGammaLoc, image_gamma);
        glUniform1i(filterTextModeLoc, text_mode);
        glUniform1f(filterZoomLoc, zoom_level);
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    glColor3f(1.0f, 1.0f, 1.0f);

    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
    glEnd();

    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);

    if (flashlight_mode) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        draw_circular_flashlight(flash_target_x, flash_target_y);
    }

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
    if (msg == WM_HOTKEY && wparam == HOTKEY_ID) {
        create_capture_window();
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

            if (key_down && win_pressed && !win_z_down) {
                win_z_down = 1;
                PostMessage(hotkey_hwnd, WM_HOTKEY, HOTKEY_ID, 0);
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

    hotkey_hwnd = create_hotkey_window(instance);
    if (!hotkey_hwnd) {
        printf("Failed to create hotkey window\n");
        return -1;
    }

    if (!RegisterHotKey(hotkey_hwnd, HOTKEY_ID, MOD_WIN | MOD_NOREPEAT, 'Z')) {
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
    else
        UnregisterHotKey(hotkey_hwnd, HOTKEY_ID);

    DestroyWindow(hotkey_hwnd);

    return 0;
}
