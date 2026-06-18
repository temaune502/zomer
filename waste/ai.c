#include "glfw\include\glad\glad.h"
#define GLFW_INCLUDE_NONE
#include "glfw\include\GLFW\glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include "glfw\include\GLFW\glfw3native.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ncnn_wrapper.h"
#define STB_IMAGE_IMPLEMENTATION
#include "glfw\stb_image.h"

// Імпортуємо функції з нашого оновленого C-API врапера
extern int init_neural_network(const char* param_path, const char* bin_path);
extern void run_neural_upscale(const unsigned char* src, int w, int h, unsigned char* dst, int dw, int dh, int tile_size, int tile_padding, int gpu_device);
extern void destroy_neural_network();

// Глобальний прапорець: 0 - шейдери (пропуск), 1 - нейронка (якісно)
int use_deep_upscale = 1; 

// Шейдери для виведення обробленої текстури на екран (OpenGL 3.3 Core)
const char* vertexShaderSource = 
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTexCoords;\n"
    "out vec2 TexCoords;\n"
    "void main() {\n"
    "   TexCoords = aTexCoords;\n"
    "   gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec2 TexCoords;\n"
    "uniform sampler2D screenTexture;\n"
    "void main() {\n"
    "   // Перевертаємо Y, бо STB читає зверху вниз, а OpenGL рахує знизу вверх\n"
    "   FragColor = texture(screenTexture, vec2(TexCoords.x, 1.0 - TexCoords.y));\n"
    "}\0";

// Допоміжна функція для збірки шейдерної програми
GLuint compile_shaders() {
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    // Перевірка на помилки компіляції Vertex шейдера
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        printf("[ERROR] Vertex Shader Compilation Failed:\n%s\n", infoLog);
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Перевірка на помилки компіляції Fragment шейдера
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        printf("[ERROR] Fragment Shader Compilation Failed:\n%s\n", infoLog);
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Перевірка лінкування програми
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        printf("[ERROR] Shader Program Linking Failed:\n%s\n", infoLog);
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

void setup_and_run(GLFWwindow* window, GLuint fboTexID, unsigned char* loaded_stb_pixels, int src_w, int src_h) {
    // 1. Ініціалізація мережі
    if (!init_neural_network("4xNomos8k_span_otf_strong.param", "4xNomos8k_span_otf_strong.bin")) {
        printf("Error: AI weights or configuration files didn't load.\n");
        return;
    }

    // Розрахунок цільових параметрів масштабування
    int scale = 4;
    int target_w = src_w * scale;
    int target_h = src_h * scale;

    // --- ПАРАМЕТРИ ДЛЯ API NCNN ---
    int tile_size = 400;       // Обробка картинки шматочками 400x400
    int tile_padding = 10;     // Заступ для приховування швів на стиках тайлів
    int gpu_device = 0;        // 0 для Intel HD Graphics, 1 для AMD Radeon (якщо доступна)

    size_t input_size = (size_t)src_w * src_h * 4;
    size_t output_size = (size_t)target_w * target_h * 4;

    unsigned char* input_pixels = malloc(input_size);
    unsigned char* output_pixels = malloc(output_size);

    if (!input_pixels || !output_pixels) {
        printf("[ERROR] Failed to allocate RAM buffers for image scaling!\n");
        if (input_pixels) free(input_pixels);
        if (output_pixels) free(output_pixels);
        return;
    }

    // Копіюємо первинні дані з STB в робочий вхідний буфер
    memcpy(input_pixels, loaded_stb_pixels, input_size);

    // Створюємо екранний прямокутник (Full-screen Quad) для виведення текстури
    GLuint shaderProgram = compile_shaders();
    
    float quadVertices[] = {
        // Позиції (X, Y)  // Текстурні координати (U, V)
        -1.0f,  1.0f,      0.0f, 1.0f,
        -1.0f, -1.0f,      0.0f, 0.0f,
         1.0f, -1.0f,      1.0f, 0.0f,

        -1.0f,  1.0f,      0.0f, 1.0f,
         1.0f, -1.0f,      1.0f, 0.0f,
         1.0f,  1.0f,      1.0f, 1.0f
    };

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // Атрибут позицій геометрії
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    
    // Атрибут текстурних координат
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);

    // Головний рендер-цикл програми
    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (use_deep_upscale) {
            // Передаємо вказівники та параметри тайлінгу безпосередньо у врапер
            run_neural_upscale(input_pixels, src_w, src_h, output_pixels, target_w, target_h, tile_size, tile_padding, gpu_device);
            
            // Завантажуємо отриманий результат у текстурну пам'ять OpenGL
            glBindTexture(GL_TEXTURE_2D, fboTexID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target_w, target_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, output_pixels);
        }

        // Візуалізація: малюємо текстуру за допомогою нашого шейдера
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, fboTexID);
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Ресурси віконного циклу та OpenGL контексту
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    free(input_pixels);
    free(output_pixels);
    destroy_neural_network();
}

int main(void) {
    printf("Starting application...\n");

    if (!glfwInit()) {
        printf("Error: Failed to initialize GLFW\n");
        return -1;
    }

    // Встановлюємо OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 960, "Custom AI Upscaler", NULL, NULL);
    if (!window) {
        printf("Error: Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Error: Failed to initialize GLAD\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Вмикаємо вертикальну синхронізацію (V-Sync)
    glfwSwapInterval(1);

    // Завантаження картинки з диска
    int img_w, img_h, channels;
    unsigned char* loaded_stb_pixels = stbi_load("input.png", &img_w, &img_h, &channels, 4);
    
    if (!loaded_stb_pixels) {
        printf("Error: Critical! Failed to load 'input.png'. Verify file availability.\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    printf("Image loaded successfully: %dx%d, original channels: %d\n", img_w, img_h, channels);

    // Створення порожньої OpenGL текстури, куди AI буде записувати кадри
    GLuint fboTexID;
    glGenTextures(1, &fboTexID);
    glBindTexture(GL_TEXTURE_2D, fboTexID);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Передаємо керування в основний блок виконання
    setup_and_run(window, fboTexID, loaded_stb_pixels, img_w, img_h);

    // Остаточне звільнення глобальних ресурсів
    stbi_image_free(loaded_stb_pixels);
    glDeleteTextures(1, &fboTexID);
    glfwDestroyWindow(window);
    glfwTerminate();

    printf("Application closed successfully.\n");
    return 0;
}