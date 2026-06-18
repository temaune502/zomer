#ifndef UI_H
#define UI_H

#include <stdbool.h>

// Forward declaration for GLFWwindow if needed
struct GLFWwindow;

typedef struct {
    float r, g, b, a;
} UIColor;

typedef struct {
    int x, y, w, h;
} UIRect;

typedef enum {
    UI_WIDGET_BUTTON,
    UI_WIDGET_COLOR_PICKER,
    UI_WIDGET_SLIDER,
    UI_WIDGET_LABEL,
    UI_WIDGET_ICON_BUTTON
} UIWidgetType;

typedef struct {
    int id;
    UIRect rect;
    bool hovered;
    bool active;
    bool clicked;
} UIState;

// Callback for drawing custom icons
typedef void (*UIIconDrawCallback)(int x, int y, int w, int h, void* userdata);

void ui_init(void);
void ui_begin(int screen_w, int screen_h, int mouse_x, int mouse_y, bool mouse_down);
void ui_end(void);
bool ui_is_hovering(void);

// Simple button with color
bool ui_button(int id, int x, int y, int w, int h, UIColor color, UIColor hover_color, bool selected);

// Button with an icon drawing callback
bool ui_icon_button(int id, int x, int y, int w, int h, UIColor color, UIColor hover_color, bool selected, UIIconDrawCallback icon_cb, void* userdata);

// Color selection button
bool ui_color_button(int id, int x, int y, int w, int h, UIColor color, bool selected);

// Slider for numeric values
bool ui_slider(int id, int x, int y, int w, int h, float* value, float min, float max, UIColor color);

// Helper functions for conversion (copied from main for consistency)
float ui_px_to_ndc_x(int px, int window_width);
float ui_px_to_ndc_y(int py, int window_height);

// Utility for drawing basic shapes in UI space
void ui_draw_rect(int x, int y, int w, int h, UIColor color);
void ui_draw_rect_outline(int x, int y, int w, int h, UIColor color, float thickness);

#endif // UI_H
