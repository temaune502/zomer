#include "ui.h"
#include "libs/glfw/include/glad/glad.h"
#include <math.h>

static struct {
    int screen_w, screen_h;
    int mouse_x, mouse_y;
    bool mouse_down;
    int hot_item;    // hovered
    int active_item; // being clicked
} ui_ctx;

void ui_init(void) {
    ui_ctx.hot_item = 0;
    ui_ctx.active_item = 0;
}

void ui_begin(int screen_w, int screen_h, int mouse_x, int mouse_y, bool mouse_down) {
    ui_ctx.screen_w = screen_w;
    ui_ctx.screen_h = screen_h;
    ui_ctx.mouse_x = mouse_x;
    ui_ctx.mouse_y = mouse_y;
    ui_ctx.mouse_down = mouse_down;
    
    // Reset hot item every frame, widgets will set it if hovered
    ui_ctx.hot_item = 0;

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ui_end(void) {
    if (!ui_ctx.mouse_down) {
        ui_ctx.active_item = 0;
    }
    glDisable(GL_BLEND);
}

bool ui_is_hovering(void) {
    return ui_ctx.hot_item != 0;
}

float ui_px_to_ndc_x(int px, int window_width) {
    return (float)px / (float)window_width * 2.0f - 1.0f;
}

float ui_px_to_ndc_y(int py, int window_height) {
    return 1.0f - (float)py / (float)window_height * 2.0f;
}

void ui_draw_rect(int x, int y, int w, int h, UIColor color) {
    float x0 = ui_px_to_ndc_x(x, ui_ctx.screen_w);
    float y0 = ui_px_to_ndc_y(y, ui_ctx.screen_h);
    float x1 = ui_px_to_ndc_x(x + w, ui_ctx.screen_w);
    float y1 = ui_px_to_ndc_y(y + h, ui_ctx.screen_h);

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
        glVertex2f(x0, y1);
        glVertex2f(x1, y1);
        glVertex2f(x1, y0);
        glVertex2f(x0, y0);
    glEnd();
}

void ui_draw_rect_outline(int x, int y, int w, int h, UIColor color, float thickness) {
    float x0 = ui_px_to_ndc_x(x, ui_ctx.screen_w);
    float y0 = ui_px_to_ndc_y(y, ui_ctx.screen_h);
    float x1 = ui_px_to_ndc_x(x + w, ui_ctx.screen_w);
    float y1 = ui_px_to_ndc_y(y + h, ui_ctx.screen_h);

    glLineWidth(thickness);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
        glVertex2f(x0, y1);
        glVertex2f(x1, y1);
        glVertex2f(x1, y0);
        glVertex2f(x0, y0);
    glEnd();
    glLineWidth(1.0f);
}

static bool ui_region_hit(int x, int y, int w, int h) {
    return ui_ctx.mouse_x >= x && ui_ctx.mouse_x < x + w &&
           ui_ctx.mouse_y >= y && ui_ctx.mouse_y < y + h;
}

bool ui_button(int id, int x, int y, int w, int h, UIColor color, UIColor hover_color, bool selected) {
    bool clicked = false;
    bool is_hot = ui_region_hit(x, y, w, h);

    if (is_hot) {
        ui_ctx.hot_item = id;
        if (ui_ctx.active_item == 0 && ui_ctx.mouse_down) {
            ui_ctx.active_item = id;
        }
    }

    if (!ui_ctx.mouse_down && ui_ctx.hot_item == id && ui_ctx.active_item == id) {
        clicked = true;
    }

    UIColor current_color = (is_hot || selected) ? hover_color : color;
    
    // Draw button background
    ui_draw_rect(x, y, w, h, current_color);
    
    // Draw selection outline if selected
    if (selected) {
        ui_draw_rect_outline(x - 2, y - 2, w + 4, h + 4, (UIColor){1.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
    }

    return clicked;
}

bool ui_icon_button(int id, int x, int y, int w, int h, UIColor color, UIColor hover_color, bool selected, UIIconDrawCallback icon_cb, void* userdata) {
    bool clicked = ui_button(id, x, y, w, h, color, hover_color, selected);
    
    if (icon_cb) {
        icon_cb(x, y, w, h, userdata);
    }
    
    return clicked;
}

bool ui_color_button(int id, int x, int y, int w, int h, UIColor color, bool selected) {
    bool clicked = false;
    bool is_hot = ui_region_hit(x, y, w, h);

    if (is_hot) {
        ui_ctx.hot_item = id;
        if (ui_ctx.active_item == 0 && ui_ctx.mouse_down) {
            ui_ctx.active_item = id;
        }
    }

    if (!ui_ctx.mouse_down && ui_ctx.hot_item == id && ui_ctx.active_item == id) {
        clicked = true;
    }

    // Draw color square
    ui_draw_rect(x, y, w, h, color);
    
    // Draw selection outline
    if (selected || is_hot) {
        ui_draw_rect_outline(x - 2, y - 2, w + 4, h + 4, (UIColor){1.0f, 1.0f, 1.0f, 1.0f}, 2.0f);
    }

    return clicked;
}

bool ui_slider(int id, int x, int y, int w, int h, float* value, float min, float max, UIColor color) {
    bool changed = false;
    bool is_hot = ui_region_hit(x, y, w, h);

    if (is_hot) {
        ui_ctx.hot_item = id;
        if (ui_ctx.active_item == 0 && ui_ctx.mouse_down) {
            ui_ctx.active_item = id;
        }
    }

    if (ui_ctx.active_item == id) {
        float relative_x = (float)(ui_ctx.mouse_x - x) / (float)w;
        if (relative_x < 0) relative_x = 0;
        if (relative_x > 1) relative_x = 1;
        *value = min + relative_x * (max - min);
        changed = true;
    }

    // Draw background track
    ui_draw_rect(x, y + h/2 - 2, w, 4, (UIColor){0.2f, 0.2f, 0.25f, 1.0f});
    
    // Draw value fill
    float fill_w = ((*value - min) / (max - min)) * w;
    ui_draw_rect(x, y + h/2 - 2, (int)fill_w, 4, color);
    
    // Draw handle
    int handle_x = x + (int)fill_w - 4;
    ui_draw_rect(handle_x, y, 8, h, (UIColor){0.95f, 0.95f, 1.0f, 1.0f});

    return changed;
}
