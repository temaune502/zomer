#ifndef UTILS_H
#define UTILS_H

float flash_target_x;
float flash_target_y;
int flash_locked;
int flip_y;
int flip_x;
int is_dragging; 
double last_mouse_x; 
double last_mouse_y ;
int flashlight_mode;
float flashlight_radius;
float zoom_level;
float offset_x; 
float offset_y;
int screen_w;
int screen_h;
int  GetScreenShot(unsigned char * pixeldata, int width, int height);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
void draw_circular_flashlight(float center_x, float center_y);
#ifdef UTILS_IMPLEMENTATION


void draw_circular_flashlight(float center_x, float center_y)
{
    float radius = flashlight_radius;

    if (radius < 60.0f) radius = 60.0f;
    if (radius > 900.0f) radius = 900.0f;

    /* Коло в пікселях: різні NDC-радіуси по осях компенсують aspect ratio */
    float inner_rx = radius / (float)screen_w * 2.0f;
    float inner_ry = radius / (float)screen_h * 2.0f;
    float outer_rx = inner_rx + 2.5f;
    float outer_ry = inner_ry + 2.5f;

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.55f);

    glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i <= 128; i++) {
        float angle = i * 2.0f * 3.1415926535f / 128.0f;
        float cos_a = cosf(angle);
        float sin_a = sinf(angle);

        glVertex2f(center_x + outer_rx * cos_a, center_y + outer_ry * sin_a);
        glVertex2f(center_x + inner_rx * cos_a, center_y + inner_ry * sin_a);
    }

    glEnd();
    glDisable(GL_BLEND);
}

    // Колбек для кліків миші
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                is_dragging = 1;
                glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
            } else if (action == GLFW_RELEASE) {
                is_dragging = 0;
            }
        }
    }

// Колбек для руху миші (коли затиснута ПКМ — рухаємо сцену)
    void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
    {
        if (is_dragging) {
            // Вираховуємо, на скільки пікселів зрушила миша
            double dx = xpos - last_mouse_x;
            double dy = ypos - last_mouse_y;

            // Переводимо піксельну дельту в простір OpenGL [-1; 1]
            // Множимо на 2.0, бо екран OpenGL має ширину 2 одиниці (від -1 до 1)
            offset_x += (float)(dx / screen_w) * 2.0f;
            offset_y -= (float)(dy / screen_h) * 2.0f; // В OpenGL Y йде вгору

            last_mouse_x = xpos;
            last_mouse_y = ypos;
        }
    }


void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    {
        if (yoffset > 0) {
            flashlight_radius *= 1.15f;
        } else if (yoffset < 0) {
            flashlight_radius /= 1.15f;
        }

        if (flashlight_radius < 60.0f) flashlight_radius = 60.0f;
        if (flashlight_radius > 900.0f) flashlight_radius = 900.0f;
        return;
    }

    int ctrl_pressed = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) || 
                       (glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);

    if (ctrl_pressed)
    {
        // Захист від некоректних розмірів
        if (screen_w <= 1.0 || screen_h <= 1.0) return;

        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);

        // Переводимо координати миші в простір OpenGL [-1.0; 1.0]
        float mouse_gl_x = (float)((mouse_x / screen_w) * 2.0 - 1.0);
        float mouse_gl_y = (float)(1.0 - (mouse_y / screen_h) * 2.0);

        // Рахуємо точку на текстурі, куди дивиться миша
        float target_x = (mouse_gl_x - offset_x) / (zoom_level * flip_x);
        float target_y = (mouse_gl_y - offset_y) / (zoom_level * flip_y);

        // Multiplicative zoom feels faster and stays stable at different scales.
        float zoom_step = 1.35f;
        if (yoffset > 0) {
            zoom_level *= zoom_step;
        } else if (yoffset < 0) {
            zoom_level /= zoom_step;
        }

        // Жорсткі обмеження:
        if (zoom_level < 0.05f) zoom_level = 0.05f; // Максимальне віддалення (5% від початкового розміру)
        if (zoom_level > 40.0f)  zoom_level = 40.0f;  // Максимальне наближення (500%)

        // Оновлюємо зсув, щоб точка під мишкою залишалася нерухомою
        offset_x = mouse_gl_x - target_x * zoom_level * flip_x;
        offset_y = mouse_gl_y - target_y * zoom_level * flip_y;
    }
}

    int GetScreenShot(unsigned char * pixeldata, int width, int height)
    {
        if(!pixeldata) return 0;

        HDC hScreenDC = GetDC(NULL);
        if (!hScreenDC) {
            return 0;
        }
        
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        if(!hMemoryDC) 
        {
            ReleaseDC(NULL, hScreenDC);
            return 0;
        }

        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        if(!hBitmap)
        {
            DeleteDC(hMemoryDC);
            ReleaseDC(NULL, hScreenDC);
            return 0;
        }

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(BITMAPINFO));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        
        // 1. ВИПРАВЛЕНО ОРІЄНТАЦІЮ: додано мінус, щоб Windows копіювала зверху-вниз
        bmi.bmiHeader.biHeight = -height; 
        
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        GetDIBits(hScreenDC, hBitmap, 0, height, pixeldata, &bmi, DIB_RGB_COLORS);

        // 2. ВИПРАВЛЕНО КОЛЬОРИ: перетворюємо BGRA на RGBA
        // Проходимо по кожному пікселю (крок 4 байти) і міняємо місцями R (індекс i+2) та B (індекс i)
        // Перетворюємо BGRA на RGBA (це ви вже зробили, кольори правильні)
        int totalBytes = width * height * 4;
        for (int i = 0; i < totalBytes; i += 4) {
            unsigned char temp = pixeldata[i];     // зберігаємо Blue
            pixeldata[i] = pixeldata[i + 2];       // записуємо Red на місце Blue
            pixeldata[i + 2] = temp;               // записуємо колишній Blue на місце Red
        }

        // Це вже не потрібно але залишив вимикаємо фліп, бо Windows через biHeight=-s_hight вже дала нам правильний порядок
        //stbi_flip_vertically_on_write(0);

        // Вивільнення ресурсів
        SelectObject(hMemoryDC, hOldBitmap); 
        DeleteObject(hBitmap);               
        DeleteDC(hMemoryDC);                 
        ReleaseDC(NULL, hScreenDC);
    
        return 1;

    }
#endif
#endif 

