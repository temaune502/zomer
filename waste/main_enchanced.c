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
unsigned int upscaleProgram = 0;
unsigned int rcasProgram = 0;
unsigned int fboID = 0;
unsigned int fboTexID = 0;
int upscaleTexelSizeLoc = -1;
int upscaleFilterModeLoc = -1;
int upscaleZoomLoc = -1;
int rcasTexelSizeLoc = -1;
int rcasSharpnessLoc = -1;
int rcasFilterModeLoc = -1;
int rcasContrastLoc = -1;
int rcasBrightnessLoc = -1;
int rcasGammaLoc = -1;
int rcasTextModeLoc = -1;
int rcasZoomLoc = -1;
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

enum {
    DRAW_TOOL_PENCIL = 0,
    DRAW_TOOL_LINE,
    DRAW_TOOL_RECT,
    DRAW_TOOL_ELLIPSE,
    DRAW_TOOL_BUCKET,
    DRAW_TOOL_ERASER,
    DRAW_TOOL_COUNT
};

#define DRAW_COLOR_COUNT 8
#define HUB_MARGIN_LEFT  12
#define HUB_WIDTH        80
#define HUB_PAD          10
#define HUB_BTN_SIZE     34
#define HUB_BTN_GAP      6
#define HUB_CLR_SIZE     26
#define HUB_CLR_GAP      5
#define DRAW_UNDO_MAX    16

static unsigned char *draw_pixels = NULL;
static unsigned int draw_tex_id = 0;
static int draw_hub_visible = 0;
static int draw_tool = DRAW_TOOL_PENCIL;
static int draw_color_idx = 0;
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

static void composite_draw_onto_rgba(unsigned char *dst, int dst_w, int dst_h, int content_x0, int content_y0);

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

static   unsigned int compile_shader(unsigned int type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    char log[1024];
    int ok = 0;

    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);

    if (!ok) {
        glGetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
        printf("Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static const char *shader_vertex_source =
    "#version 120\n"
    "varying vec2 vTexCoord;\n"
    "void main() {\n"
    "    vTexCoord = gl_MultiTexCoord0.xy;\n"
    "    gl_Position = ftransform();\n"
    "}\n";

static unsigned int link_shader_program(const char *fragment_source)
{
    unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, shader_vertex_source);
    unsigned int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    unsigned int program = 0;
    int ok = 0;

    if (!vertex_shader || !fragment_shader) {
        if (vertex_shader) glDeleteShader(vertex_shader);
        if (fragment_shader) glDeleteShader(fragment_shader);
        return 0;
    }

    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(program, (GLsizei)sizeof(log), NULL, log);
        printf("Shader link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static int create_upscale_program(void)
{
    const char *fragment_source =
        "#version 120\n"
        "uniform sampler2D screenTex;\n"
        "uniform vec2 texelSize;\n"
        "uniform float sharpness;\n"
        "uniform int filterMode;\n"
        "uniform float zoomLevel;\n"
        "varying vec2 vTexCoord;\n"
        "vec2 upscale_uv(vec2 tc) { return vec2(tc.x, 1.0 - tc.y); }\n"
        "void main() {\n"
        "    vec2 uv = upscale_uv(vTexCoord);\n"
        "    vec4 color = texture2D(screenTex, uv);\n"
        "    \n"
        "    if (filterMode == 4 || (filterMode == 2 && zoomLevel > 1.4)) {\n"
        "        vec2 fract_uv = uv * (1.0 / texelSize) - 0.5;\n"
        "        vec2 iuv = floor(fract_uv);\n"
        "        vec2 fuv = fract(fract_uv);\n"
        "        \n"
        "        vec2 w0 = 0.5 * (-fuv + 2.0 * fuv * fuv - fuv * fuv * fuv);\n"
        "        vec2 w1 = 0.5 * (2.0 + -5.0 * fuv * fuv + 3.0 * fuv * fuv * fuv);\n"
        "        vec2 w2 = 0.5 * (fuv + 4.0 * fuv * fuv - 3.0 * fuv * fuv * fuv);\n"
        "        vec2 w3 = 0.5 * (-fuv * fuv + fuv * fuv * fuv);\n"
        "        \n"
        "        vec4 centerValue = vec4(0.0);\n"
        "        vec4 minColor = vec4(1.0);\n"
        "        vec4 maxColor = vec4(0.0);\n"
        "        \n"
        "        for(int j = -1; j <= 2; j++) {\n"
        "            for(int i = -1; i <= 2; i++) {\n"
        "                vec2 offset = vec2(float(i), float(j));\n"
        "                vec4 sampleTex = texture2D(screenTex, (iuv + vec2(0.5) + offset) * texelSize);\n"
        "                \n"
        "                float weight = (i == -1 ? w0.x : i == 0 ? w1.x : i == 1 ? w2.x : w3.x) *\n"
        "                               (j == -1 ? w0.y : j == 0 ? w1.y : j == 1 ? w2.y : w3.y);\n"
        "                \n"
        "                centerValue += sampleTex * weight;\n"
        "                \n"
        "                if (i >= 0 && i <= 1 && j >= 0 && j <= 1) {\n"
        "                    minColor = min(minColor, sampleTex);\n"
        "                    maxColor = max(maxColor, sampleTex);\n"
        "                }\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        color = clamp(centerValue, minColor, maxColor);\n"
        "    }\n"
        "    \n"
        "    gl_FragColor = color;\n"
        "}\n";

    upscaleProgram = link_shader_program(fragment_source);
    if (!upscaleProgram)
        return 0;

    glUseProgram(upscaleProgram);
    glUniform1i(glGetUniformLocation(upscaleProgram, "screenTex"), 0);
    upscaleTexelSizeLoc = glGetUniformLocation(upscaleProgram, "texelSize");
    upscaleFilterModeLoc = glGetUniformLocation(upscaleProgram, "filterMode");
    upscaleZoomLoc = glGetUniformLocation(upscaleProgram, "zoomLevel");
    glUseProgram(0);

    return 1;
}

static int create_rcas_program(void)
{
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
        "    vec2 uv = vTexCoord;\n"
        "    vec4 color = texture2D(screenTex, uv);\n"
        "    \n"
        "    vec4 left  = texture2D(screenTex, uv + vec2(-texelSize.x, 0.0));\n"
        "    vec4 right = texture2D(screenTex, uv + vec2(texelSize.x, 0.0));\n"
        "    vec4 up    = texture2D(screenTex, uv + vec2(0.0, -texelSize.y));\n"
        "    vec4 down  = texture2D(screenTex, uv + vec2(0.0, texelSize.y));\n"
        "    \n"
        "    vec4 minNeighbour = min(color, min(min(left, right), min(up, down)));\n"
        "    vec4 maxNeighbour = max(color, max(max(left, right), max(up, down)));\n"
        "    vec4 blur = (left + right + up + down) * 0.25;\n"
        "    float zoomSharpness = zoomLevel > 1.2 ? sharpness : sharpness * 0.35;\n"
        "    \n"
        "    if (filterMode == 1) {\n"
        "        color = color * 0.68 + blur * 0.32;\n"
        "    } else if (filterMode == 2) {\n"
        "        color = color + (color - blur) * zoomSharpness;\n"
        "    } else if (filterMode == 4) {\n"
        "        vec4 edgeDiff = maxNeighbour - minNeighbour;\n"
        "        vec4 edgeWeight = clamp(edgeDiff * 2.0, 0.0, 1.0);\n"
        "        color = color + (color - blur) * (zoomSharpness * 1.5) * edgeWeight;\n"
        "    }\n"
        "    \n"
        "    if (textMode == 1) {\n"
        "        float luma = dot(color.rgb, vec3(0.299, 0.587, 0.114));\n"
        "        vec3 gray = vec3(luma);\n"
        "        color.rgb = mix(color.rgb, gray, 0.20);\n"
        "        \n"
        "        vec4 textEdge = clamp((color - blur) * 2.5, -0.15, 0.15);\n"
        "        color.rgb += textEdge.rgb;\n"
        "        color.rgb = (color.rgb - 0.5) * 1.15 + 0.5;\n"
        "    }\n"
        "    \n"
        "    color.rgb = (color.rgb - 0.5) * contrast + 0.5 + brightness;\n"
        "    color.rgb = pow(clamp(color.rgb, 0.0, 1.0), vec3(1.0 / gammaValue));\n"
        "    gl_FragColor = vec4(clamp(color.rgb, 0.0, 1.0), color.a);\n"
        "}\n";

    rcasProgram = link_shader_program(fragment_source);
    if (!rcasProgram)
        return 0;

    glUseProgram(rcasProgram);
    glUniform1i(glGetUniformLocation(rcasProgram, "screenTex"), 0);
    rcasTexelSizeLoc = glGetUniformLocation(rcasProgram, "texelSize");
    rcasSharpnessLoc = glGetUniformLocation(rcasProgram, "sharpness");
    rcasFilterModeLoc = glGetUniformLocation(rcasProgram, "filterMode");
    rcasContrastLoc = glGetUniformLocation(rcasProgram, "contrast");
    rcasBrightnessLoc = glGetUniformLocation(rcasProgram, "brightness");
    rcasGammaLoc = glGetUniformLocation(rcasProgram, "gammaValue");
    rcasTextModeLoc = glGetUniformLocation(rcasProgram, "textMode");
    rcasZoomLoc = glGetUniformLocation(rcasProgram, "zoomLevel");
    glUseProgram(0);

    return 1;
}

static int init_render_pipeline(void)
{
    if (!create_upscale_program() || !create_rcas_program())
        return 0;

    glGenFramebuffers(1, &fboID);
    glGenTextures(1, &fboTexID);

    glBindTexture(GL_TEXTURE_2D, fboTexID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_width, screen_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTexID, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        printf("FBO incomplete: 0x%x\n", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    return 1;
}

static   void apply_filter_mode(void)
{
    int texture_filter = (filter_mode == FILTER_PIXELATED) ? GL_NEAREST : GL_LINEAR;

    if (!textureID)
        return;

    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);
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

static void render_upscale_pass(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fboID);
    glViewport(0, 0, screen_width, screen_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(offset_x, offset_y, 0.0f);
    glScalef(zoom_level * (float)flip_x, zoom_level * (float)flip_y, 1.0f);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (filter_mode == FILTER_PIXELATED) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glColor3f(1.0f, 1.0f, 1.0f);

    if (upscaleProgram) {
        glUseProgram(upscaleProgram);
        glUniform2f(upscaleTexelSizeLoc, 1.0f / (float)screen_width, 1.0f / (float)screen_height);
        glUniform1i(upscaleFilterModeLoc, filter_mode);
        glUniform1f(upscaleZoomLoc, zoom_level);
    }

    draw_textured_quad();
    glUseProgram(0);
}

static void render_rcas_pass(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, screen_width, screen_height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, fboTexID);
    glColor3f(1.0f, 1.0f, 1.0f);

    if (rcasProgram) {
        glUseProgram(rcasProgram);
        glUniform2f(rcasTexelSizeLoc, 1.0f / (float)screen_width, 1.0f / (float)screen_height);
        glUniform1f(rcasSharpnessLoc, 0.45f);
        glUniform1i(rcasFilterModeLoc, filter_mode);
        glUniform1f(rcasContrastLoc, image_contrast);
        glUniform1f(rcasBrightnessLoc, image_brightness);
        glUniform1f(rcasGammaLoc, image_gamma);
        glUniform1i(rcasTextModeLoc, text_mode);
        glUniform1f(rcasZoomLoc, zoom_level);
    }

    draw_textured_quad();
    glUseProgram(0);
    glDisable(GL_TEXTURE_2D);
}

static inline float px_to_ndc_x(int px)
{
    return (float)px / (float)screen_w * 2.0f - 1.0f;
}

static inline float px_to_ndc_y(int py)
{
    return 1.0f - (float)py / (float)screen_h * 2.0f;
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

static void draw_hub_layout(int *hub_x, int *hub_y, int *hub_w, int *hub_h)
{
    int tools_h = DRAW_TOOL_COUNT * (HUB_BTN_SIZE + HUB_BTN_GAP) - HUB_BTN_GAP;
    int colors_h = 4 * (HUB_CLR_SIZE + HUB_CLR_GAP) - HUB_CLR_GAP;
    int inner_h = HUB_PAD + tools_h + 14 + colors_h + 10 + HUB_BTN_SIZE + 10 + HUB_BTN_SIZE + HUB_PAD;

    *hub_w = HUB_WIDTH;
    *hub_h = inner_h;
    *hub_x = HUB_MARGIN_LEFT;
    *hub_y = (screen_h - inner_h) / 2;
    if (*hub_y < 8)
        *hub_y = 8;
}

static int draw_hub_hit_test(int px, int py)
{
    int hub_x, hub_y, hub_w, hub_h;
    int i, col, row, bx, by;

    if (!draw_hub_visible)
        return -1;

    draw_hub_layout(&hub_x, &hub_y, &hub_w, &hub_h);
    if (px < hub_x || px >= hub_x + hub_w || py < hub_y || py >= hub_y + hub_h)
        return -1;

    bx = hub_x + HUB_PAD;
    by = hub_y + HUB_PAD;

    for (i = 0; i < DRAW_TOOL_COUNT; i++) {
        int ty = by + i * (HUB_BTN_SIZE + HUB_BTN_GAP);
        if (px >= bx && px < bx + HUB_BTN_SIZE && py >= ty && py < ty + HUB_BTN_SIZE)
            return i;
    }

    by += DRAW_TOOL_COUNT * (HUB_BTN_SIZE + HUB_BTN_GAP) + 14;

    for (i = 0; i < DRAW_COLOR_COUNT; i++) {
        col = i % 2;
        row = i / 2;
        int cx = bx + col * (HUB_CLR_SIZE + HUB_CLR_GAP);
        int cy = by + row * (HUB_CLR_SIZE + HUB_CLR_GAP);
        if (px >= cx && px < cx + HUB_CLR_SIZE && py >= cy && py < cy + HUB_CLR_SIZE)
            return 100 + i;
    }

    by += 4 * (HUB_CLR_SIZE + HUB_CLR_GAP) + 10;

    if (px >= bx && px < bx + 28 && py >= by && py < by + 28)
        return 301;
    if (px >= bx + 36 && px < bx + 64 && py >= by && py < by + 28)
        return 302;

    by += HUB_BTN_SIZE + 10;
    if (px >= bx && px < bx + HUB_BTN_SIZE && py >= by && py < by + HUB_BTN_SIZE)
        return 200;

    return 300;
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
    int *stack_x = (int*)malloc(stack_cap * sizeof(int) * 2);
    size_t stack_size = 0;
    unsigned char target[4];
    size_t idx;
    int x, y, i;

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

    draw_margin = screen_width / 2;
    if (draw_margin < 256)
        draw_margin = 256;
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
    glColor3f(1.0f, 1.0f, 1.0f);
    draw_content_textured_quad(-draw_margin, -draw_margin,
                               screen_width + draw_margin, screen_height + draw_margin);

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

static void draw_hub_rect_px(int x, int y, int w, int h, float r, float g, float b, float a)
{
    float x0 = px_to_ndc_x(x);
    float y0 = px_to_ndc_y(y);
    float x1 = px_to_ndc_x(x + w);
    float y1 = px_to_ndc_y(y + h);

    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2f(x0, y1);
        glVertex2f(x1, y1);
        glVertex2f(x1, y0);
        glVertex2f(x0, y0);
    glEnd();
}

static void draw_hub_rect_outline_px(int x, int y, int w, int h, float r, float g, float b)
{
    float x0 = px_to_ndc_x(x);
    float y0 = px_to_ndc_y(y);
    float x1 = px_to_ndc_x(x + w);
    float y1 = px_to_ndc_y(y + h);

    glColor3f(r, g, b);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y1);
        glVertex2f(x1, y1);
        glVertex2f(x1, y0);
        glVertex2f(x0, y0);
    glEnd();
}

static void draw_hub_tool_icon(int tool, int bx, int ty)
{
    int cx = bx + HUB_BTN_SIZE / 2;
    int cy = ty + HUB_BTN_SIZE / 2;

    glColor3f(0.95f, 0.96f, 1.0f);
    glLineWidth(2.2f);

    if (tool == DRAW_TOOL_PENCIL) {
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(bx + 8), px_to_ndc_y(ty + 26));
            glVertex2f(px_to_ndc_x(bx + 24), px_to_ndc_y(ty + 10));
        glEnd();
        draw_hub_rect_px(bx + 21, ty + 7, 5, 5, 0.95f, 0.78f, 0.25f, 1.0f);
    } else if (tool == DRAW_TOOL_LINE) {
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(bx + 7), px_to_ndc_y(ty + 24));
            glVertex2f(px_to_ndc_x(bx + 27), px_to_ndc_y(ty + 10));
        glEnd();
        glPointSize(4.0f);
        glBegin(GL_POINTS);
            glVertex2f(px_to_ndc_x(bx + 7), px_to_ndc_y(ty + 24));
            glVertex2f(px_to_ndc_x(bx + 27), px_to_ndc_y(ty + 10));
        glEnd();
        glPointSize(1.0f);
    } else if (tool == DRAW_TOOL_RECT) {
        draw_hub_rect_outline_px(bx + 7, ty + 7, 20, 20, 0.95f, 0.96f, 1.0f);
    } else if (tool == DRAW_TOOL_ELLIPSE) {
        int s;
        glBegin(GL_LINE_LOOP);
        for (s = 0; s < 32; s++) {
            float ang = s * 6.2831853f / 32.0f;
            glVertex2f(px_to_ndc_x(cx + (int)(11.0f * cosf(ang))),
                       px_to_ndc_y(cy + (int)(11.0f * sinf(ang))));
        }
        glEnd();
    } else if (tool == DRAW_TOOL_BUCKET) {
        draw_hub_rect_px(bx + 9, ty + 14, 16, 12, 0.55f, 0.62f, 0.82f, 1.0f);
        glBegin(GL_LINE_LOOP);
            glVertex2f(px_to_ndc_x(bx + 9), px_to_ndc_y(ty + 14));
            glVertex2f(px_to_ndc_x(bx + 25), px_to_ndc_y(ty + 14));
            glVertex2f(px_to_ndc_x(bx + 23), px_to_ndc_y(ty + 26));
            glVertex2f(px_to_ndc_x(bx + 11), px_to_ndc_y(ty + 26));
        glEnd();
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(bx + 16), px_to_ndc_y(ty + 8));
            glVertex2f(px_to_ndc_x(bx + 16), px_to_ndc_y(ty + 14));
            glVertex2f(px_to_ndc_x(bx + 12), px_to_ndc_y(ty + 10));
            glVertex2f(px_to_ndc_x(bx + 20), px_to_ndc_y(ty + 10));
        glEnd();
        glBegin(GL_TRIANGLES);
            glColor3f(0.35f, 0.75f, 1.0f);
            glVertex2f(px_to_ndc_x(bx + 13), px_to_ndc_y(ty + 22));
            glVertex2f(px_to_ndc_x(bx + 21), px_to_ndc_y(ty + 22));
            glVertex2f(px_to_ndc_x(bx + 17), px_to_ndc_y(ty + 18));
        glEnd();
    } else if (tool == DRAW_TOOL_ERASER) {
        draw_hub_rect_px(bx + 8, ty + 11, 18, 13, 0.92f, 0.70f, 0.78f, 1.0f);
        draw_hub_rect_outline_px(bx + 8, ty + 11, 18, 13, 0.98f, 0.98f, 1.0f);
        glBegin(GL_LINES);
            glVertex2f(px_to_ndc_x(bx + 11), px_to_ndc_y(ty + 24));
            glVertex2f(px_to_ndc_x(bx + 23), px_to_ndc_y(ty + 24));
        glEnd();
    }

    glLineWidth(1.0f);
}

static void render_draw_hub(void)
{
    int hub_x, hub_y, hub_w, hub_h;
    int bx, by, i, col, row;

    if (!draw_hub_visible)
        return;

    draw_hub_layout(&hub_x, &hub_y, &hub_w, &hub_h);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    draw_hub_rect_px(hub_x, hub_y, hub_w, hub_h, 0.12f, 0.12f, 0.14f, 0.88f);
    draw_hub_rect_outline_px(hub_x, hub_y, hub_w, hub_h, 0.45f, 0.45f, 0.50f);

    bx = hub_x + HUB_PAD;
    by = hub_y + HUB_PAD;

    for (i = 0; i < DRAW_TOOL_COUNT; i++) {
        int ty = by + i * (HUB_BTN_SIZE + HUB_BTN_GAP);
        float tr = 0.28f, tg = 0.28f, tb = 0.32f;

        if (draw_tool == i)
            tr = 0.42f, tg = 0.42f, tb = 0.48f;

        draw_hub_rect_px(bx, ty, HUB_BTN_SIZE, HUB_BTN_SIZE, tr, tg, tb, 0.95f);
        if (draw_tool == i)
            draw_hub_rect_outline_px(bx - 2, ty - 2, HUB_BTN_SIZE + 4, HUB_BTN_SIZE + 4, 1.0f, 1.0f, 1.0f);

        draw_hub_tool_icon(i, bx, ty);
    }

    by += DRAW_TOOL_COUNT * (HUB_BTN_SIZE + HUB_BTN_GAP) + 14;

    for (i = 0; i < DRAW_COLOR_COUNT; i++) {
        const unsigned char *c = draw_palette[i];
        col = i % 2;
        row = i / 2;
        int cx = bx + col * (HUB_CLR_SIZE + HUB_CLR_GAP);
        int cy = by + row * (HUB_CLR_SIZE + HUB_CLR_GAP);

        draw_hub_rect_px(cx, cy, HUB_CLR_SIZE, HUB_CLR_SIZE,
            c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, 1.0f);
        if (draw_color_idx == i)
            draw_hub_rect_outline_px(cx - 2, cy - 2, HUB_CLR_SIZE + 4, HUB_CLR_SIZE + 4, 1.0f, 1.0f, 1.0f);
    }

    by += 4 * (HUB_CLR_SIZE + HUB_CLR_GAP) + 10;

    draw_hub_rect_px(bx, by, 28, 28, 0.22f, 0.22f, 0.26f, 0.95f);
    draw_hub_rect_px(bx + 36, by, 28, 28, 0.22f, 0.22f, 0.26f, 0.95f);
    glColor3f(0.95f, 0.95f, 1.0f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
        glVertex2f(px_to_ndc_x(bx + 14), px_to_ndc_y(by + 18));
        glVertex2f(px_to_ndc_x(bx + 14), px_to_ndc_y(by + 10));
        glVertex2f(px_to_ndc_x(bx + 50), px_to_ndc_y(by + 10));
        glVertex2f(px_to_ndc_x(bx + 50), px_to_ndc_y(by + 18));
    glEnd();
    glLineWidth(1.0f);
    {
        int preview = draw_brush_size;
        if (preview > 10) preview = 10;
        draw_hub_rect_px(bx + 30 - preview / 2, by + 14 - preview / 2, preview + 2, preview + 2,
            draw_palette[draw_color_idx][0] / 255.0f,
            draw_palette[draw_color_idx][1] / 255.0f,
            draw_palette[draw_color_idx][2] / 255.0f, 1.0f);
    }

    by += HUB_BTN_SIZE + 10;
    draw_hub_rect_px(bx, by, HUB_BTN_SIZE, HUB_BTN_SIZE, 0.50f, 0.18f, 0.18f, 0.95f);
    glColor3f(1.0f, 0.85f, 0.85f);
    glBegin(GL_LINES);
        glVertex2f(px_to_ndc_x(bx + 10), px_to_ndc_y(by + 17));
        glVertex2f(px_to_ndc_x(bx + 24), px_to_ndc_y(by + 17));
        glVertex2f(px_to_ndc_x(bx + 17), px_to_ndc_y(by + 10));
        glVertex2f(px_to_ndc_x(bx + 17), px_to_ndc_y(by + 24));
    glEnd();

    glDisable(GL_BLEND);
}

#define HELP_PANEL_W 580
#define HELP_PANEL_H 500
#define HELP_LINE_H  19

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

    draw_hub_rect_px(px, py, HELP_PANEL_W, HELP_PANEL_H, 0.07f, 0.07f, 0.09f, 0.94f);
    draw_hub_rect_outline_px(px, py, HELP_PANEL_W, HELP_PANEL_H, 0.55f, 0.55f, 0.62f);

    glDisable(GL_BLEND);
}

static inline void help_overlay_destroy_font(void)
{
    if (help_overlay_font) {
        DeleteObject(help_overlay_font);
        help_overlay_font = NULL;
    }
}

static void render_help_overlay_text(void)
{
    HWND hwnd;
    HDC hdc;
    HFONT old_font;
    int px, py, x, y, i;
    static const wchar_t *lines[] = {
        L"Zomer \u2014 \u043a\u0435\u0440\u0443\u0432\u0430\u043d\u043d\u044f",
        L"",
        L"Win+Z              \u0412\u0456\u0434\u043a\u0440\u0438\u0442\u0438 \u043b\u0443\u043f\u0443",
        L"Esc                \u0417\u0430\u043a\u0440\u0438\u0442\u0438 \u043b\u0443\u043f\u0443",
        L"`                  \u0426\u044f \u043f\u0456\u0434\u043a\u0430\u0437\u043a\u0430",
        L"R                  \u0421\u043a\u0438\u043d\u0443\u0442\u0438 \u043c\u0430\u0441\u0448\u0442\u0430\u0431 \u0456 \u0437\u0441\u0443\u0432",
        L"F                  \u0424\u0456\u043b\u044c\u0442\u0440 (Off/Smooth/Sharp/Pixel/Lanczos)",
        L"Ctrl + \u043a\u043e\u043b\u0456\u0449\u0435      \u0417\u0443\u043c \u043d\u0430 \u043a\u0443\u0440\u0441\u043e\u0440\u0456",
        L"\u041f\u041a\u041c + \u0440\u0443\u0445          \u041f\u0435\u0440\u0435\u043c\u0456\u0441\u0442\u0438\u0442\u0438 \u0437\u043e\u0431\u0440\u0430\u0436\u0435\u043d\u043d\u044f",
        L"X / Z              \u0412\u0456\u0434\u0437\u0435\u0440\u043a\u0430\u043b\u0438\u0442\u0438 X / Y",
        L"",
        L"C                  \u0417\u0431\u0435\u0440\u0435\u0433\u0442\u0438 \u0441\u043a\u0440\u0456\u043d + \u043c\u0430\u043b\u044e\u043d\u043e\u043a",
        L"Shift+C            \u0417\u0431\u0435\u0440\u0435\u0433\u0442\u0438 \u043f\u043e\u0442\u043e\u0447\u043d\u0438\u0439 \u0432\u0438\u0433\u043b\u044f\u0434",
        L"Ctrl+C / P         \u041a\u043e\u043f\u0456\u044e\u0432\u0430\u0442\u0438 \u0432\u0438\u0433\u043b\u044f\u0434 \u0432 \u0431\u0443\u0444\u0435\u0440",
        L"",
        L"T                  \u0420\u0435\u0436\u0438\u043c \u0447\u0438\u0442\u0430\u043d\u043d\u044f \u0442\u0435\u043a\u0441\u0442\u0443",
        L"B / N              \u042f\u0441\u043a\u0440\u0430\u0432\u0456\u0441\u0442\u044c + / -",
        L"V / M              \u041a\u043e\u043d\u0442\u0440\u0430\u0441\u0442 + / -",
        L"G / H              \u0413\u0430\u043c\u0430 + / -",
        L"",
        L"Tab                \u041f\u0430\u043d\u0435\u043b\u044c \u043c\u0430\u043b\u044e\u0432\u0430\u043d\u043d\u044f",
        L"\u041b\u041a\u041c                \u041c\u0430\u043b\u044e\u0432\u0430\u0442\u0438 (\u043a\u043e\u043b\u0438 Tab \u0443\u0432\u0456\u043c\u043a.)",
        L"[ / ]              \u0422\u043e\u0432\u0449\u0438\u043d\u0430 \u043f\u0435\u043d\u0437\u043b\u044f / \u043b\u0456\u043d\u0456\u0439",
        L"Ctrl+Z             \u0421\u043a\u0430\u0441\u0443\u0432\u0430\u0442\u0438 \u043c\u0430\u043b\u044e\u0432\u0430\u043d\u043d\u044f",
        L"D + \u043a\u043e\u043b\u0456\u0449\u0435       \u041b\u0456\u0445\u0442\u0430\u0440\u0438\u043a / \u0440\u043e\u0437\u043c\u0456\u0440",
        NULL
    };

    if (!help_overlay_visible || !window)
        return;

    if (!help_overlay_font) {
        help_overlay_font = CreateFontW(
            -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI"
        );
        if (!help_overlay_font)
            return;
    }

    hwnd = glfwGetWin32Window(window);
    hdc = GetDC(hwnd);
    if (!hdc)
        return;

    old_font = (HFONT)SelectObject(hdc, help_overlay_font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(232, 234, 240));

    px = (screen_w - HELP_PANEL_W) / 2;
    py = (screen_h - HELP_PANEL_H) / 2;
    if (px < 8) px = 8;
    if (py < 8) py = 8;

    x = px + 22;
    y = py + 18;

    for (i = 0; lines[i]; i++) {
        if (lines[i][0])
            TextOutW(hdc, x, y, lines[i], (int)lstrlenW(lines[i]));
        y += HELP_LINE_H;
    }

    SelectObject(hdc, old_font);
    ReleaseDC(hwnd, hdc);
}

static void render_help_overlay(void)
{
    if (!help_overlay_visible)
        return;

    render_help_panel_bg();
    glFlush();
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

    color = draw_palette[draw_color_idx];
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

    if (lmb && !draw_lmb_was) {
        hit = draw_hub_hit_test(screen_px, screen_py);
        if (hit >= 0 && hit < DRAW_TOOL_COUNT) {
            draw_tool = hit;
            draw_lmb_was = lmb;
            return;
        }
        if (hit >= 100 && hit < 100 + DRAW_COLOR_COUNT) {
            draw_color_idx = hit - 100;
            draw_lmb_was = lmb;
            return;
        }
        if (hit == 200) {
            draw_clear_layer();
            draw_lmb_was = lmb;
            return;
        }
        if (hit == 301) {
            if (draw_brush_size > 1) draw_brush_size--;
            draw_lmb_was = lmb;
            return;
        }
        if (hit == 302) {
            if (draw_brush_size < 48) draw_brush_size++;
            draw_lmb_was = lmb;
            return;
        }
        if (hit >= 0) {
            draw_lmb_was = lmb;
            return;
        }
    }

    if (draw_hub_visible && draw_hub_hit_test(screen_px, screen_py) >= 0) {
        draw_lmb_was = lmb;
        draw_stroke_active = 0;
        return;
    }

    if (is_dragging) {
        draw_lmb_was = lmb;
        draw_stroke_active = 0;
        return;
    }

    color = draw_palette[draw_color_idx];
    erase = (draw_tool == DRAW_TOOL_ERASER);
    brush = draw_tool_brush_radius();

    if (lmb) {
        if (!draw_lmb_was) {
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

    if (fboID) {
        glDeleteFramebuffers(1, &fboID);
        fboID = 0;
    }

    if (fboTexID) {
        glDeleteTextures(1, &fboTexID);
        fboTexID = 0;
    }

    if (upscaleProgram) {
        glDeleteProgram(upscaleProgram);
        upscaleProgram = 0;
        upscaleTexelSizeLoc = -1;
        upscaleFilterModeLoc = -1;
        upscaleZoomLoc = -1;
    }

    if (rcasProgram) {
        glDeleteProgram(rcasProgram);
        rcasProgram = 0;
        rcasTexelSizeLoc = -1;
        rcasSharpnessLoc = -1;
        rcasFilterModeLoc = -1;
        rcasContrastLoc = -1;
        rcasBrightnessLoc = -1;
        rcasGammaLoc = -1;
        rcasTextModeLoc = -1;
        rcasZoomLoc = -1;
    }

    destroy_draw_layer();
    help_overlay_destroy_font();
    help_overlay_visible = 0;

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

    if (!init_render_pipeline()) {
        destroy_capture_window();
        printf("Failed to initialize render pipeline\n");
        return 0;
    }

    if (!init_draw_layer()) {
        destroy_capture_window();
        printf("Failed to initialize draw layer\n");
        return 0;
    }

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

    static int tab_released = 1;
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        if (tab_released) {
            draw_hub_visible = !draw_hub_visible;
            tab_released = 0;
        }
    } else {
        tab_released = 1;
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

    if (fboID && fboTexID && upscaleProgram && rcasProgram) {
        render_upscale_pass();
        render_rcas_pass();
    }

    if (draw_dirty)
        upload_draw_texture();

    render_draw_layer();
    render_shape_preview();

    if (flashlight_mode) {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        draw_circular_flashlight(flash_target_x, flash_target_y);
    }

    if (draw_hub_visible)
        render_draw_hub();

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
