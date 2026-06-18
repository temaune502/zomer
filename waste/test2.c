#define GLAD_GL_IMPLEMENTATION
#include "glfw\include\glad\glad.h"
#define GLFW_INCLUDE_NONE
#include "glfw\include\GLFW\glfw3.h"
#include <stdio.h>


// Вершинний шейдер (просто передає координати далі)
const char* vertexShaderSource = 
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main() {\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

// Фрагментний шейдер (фарбує все в білий колір)
const char* fragmentShaderSource = 
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "   FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n"
    "}\0";

// Обробка зміни розміру вікна
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

int main() {
    // 1. Ініціалізація GLFW
    if (!glfwInit()) {
        printf("Помилка ініціалізації GLFW\n");
        return -1;
    }

    // Налаштування OpenGL версії 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 2. Створення вікна
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Square (C)", NULL, NULL);
    if (!window) {
        printf("Не вдалося створити вікно GLFW\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // 3. Ініціалізація GLAD (завантаження функцій OpenGL)
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Не вдалося ініціалізувати GLAD\n");
        return -1;
    }

    // 4. Компіляція шейдерів
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Створення шейдерної програми
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Видаляємо шейдери, бо вони вже злінковані в програму
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 5. Координати квадрата (два трикутники)
    float vertices[] = {
        // Перший трикутник
        -0.5f,  0.5f, 0.0f,  // Топ-ліво
        -0.5f, -0.5f, 0.0f,  // Бот-ліво
         0.5f, -0.5f, 0.0f,  // Бот-право

        // Другий трикутник
        -0.5f,  0.5f, 0.0f,  // Топ-ліво
         0.5f, -0.5f, 0.0f,  // Бот-право
         0.5f,  0.5f, 0.0f   // Топ-право
    };

    // 6. Створення VAO та VBO
    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Відв'язуємо буфери
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    glBindVertexArray(0); 

    // 7. Головний цикл рендерингу
    while (!glfwWindowShouldClose(window)) {
        // Обробка вводу (ESC для виходу)
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, 1);

        // Очищення екрана (темно-сірий колір)
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Малювання квадрата
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6); // 6 вершин для 2 трикутників

        // Обмін буферів та події
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 8. Очищення ресурсів
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}