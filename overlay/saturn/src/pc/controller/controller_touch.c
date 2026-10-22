#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ultra64.h>

#include "controller_api.h"
#include "controller_touch.h"

#define LAYOUT_FILE "touch_layout.cfg"
#define MAX_UI_RECTS 48

TouchButton touch_buttons[TB_COUNT];
float touch_stick_x, touch_stick_y, touch_stick_size;
float touch_stick_ox, touch_stick_oy;
float touch_stick_dx, touch_stick_dy;
int   touch_stick_active = 0;

float touch_tool_x[2] = { 0.835f, 0.915f };
float touch_tool_y = 0.085f;
float touch_tool_r = 0.045f;
int   touch_tool_down[2] = { 0, 0 };
static SDL_FingerID tool_finger[2];
static int pending_action = TA_NONE;

int   touch_hide_state = 0;
int   touch_enabled = 1;
int   touch_edit = 0;
int   touch_selected = -2;
int   touch_menu_open = 0;
float touch_opacity = 0.70f;
float touch_ui_mult = 1.0f;
int   touch_ui_capture = 0;

static SDL_FingerID stick_finger = 0;
static SDL_FingerID drag_finger = 0;
static int dragging = 0;

static float ui_rects[MAX_UI_RECTS][4];
static int ui_rect_count = 0;

void touch_ui_rects_begin(void) { ui_rect_count = 0; }

void touch_ui_rect_add(float x0, float y0, float x1, float y1) {
    if (ui_rect_count >= MAX_UI_RECTS) return;
    ui_rects[ui_rect_count][0] = x0;
    ui_rects[ui_rect_count][1] = y0;
    ui_rects[ui_rect_count][2] = x1;
    ui_rects[ui_rect_count][3] = y1;
    ui_rect_count++;
}

static int point_in_ui(float nx, float ny) {
    for (int i = 0; i < ui_rect_count; i++) {
        if (nx >= ui_rects[i][0] && nx <= ui_rects[i][2] && ny >= ui_rects[i][1] && ny <= ui_rects[i][3]) return 1;
    }
    return 0;
}

int touch_take_action(void) {
    int a = pending_action;
    pending_action = TA_NONE;
    return a;
}

void touch_layout_reset(void) {
    // N64 colours are applied by the drawing code (A blue, B green, C yellow, Z grey, Start red).
    static const struct { const char *name, *label; float x, y, size; unsigned int mask; } def[TB_COUNT] = {
        { "A",      "A",     0.915f, 0.70f, 0.095f, A_BUTTON },
        { "B",      "B",     0.815f, 0.83f, 0.080f, B_BUTTON },
        { "Z",      "Z",     0.715f, 0.72f, 0.072f, Z_TRIG },
        { "START",  "START", 0.500f, 0.92f, 0.052f, START_BUTTON },
        { "CUP",    "^",     0.885f, 0.27f, 0.052f, U_CBUTTONS },
        { "CDOWN",  "v",     0.885f, 0.47f, 0.052f, D_CBUTTONS },
        { "CLEFT",  "<",     0.815f, 0.37f, 0.052f, L_CBUTTONS },
        { "CRIGHT", ">",     0.955f, 0.37f, 0.052f, R_CBUTTONS },
        { "L",      "L",     0.060f, 0.38f, 0.065f, L_TRIG },
        { "R",      "R",     0.735f, 0.50f, 0.060f, R_TRIG },
    };
    for (int i = 0; i < TB_COUNT; i++) {
        touch_buttons[i].name = def[i].name;
        touch_buttons[i].label = def[i].label;
        touch_buttons[i].x = def[i].x;
        touch_buttons[i].y = def[i].y;
        touch_buttons[i].size = def[i].size;
        touch_buttons[i].mask = def[i].mask;
        touch_buttons[i].pressed = 0;
        touch_buttons[i].visible = 1;
    }
    touch_stick_x = 0.14f;
    touch_stick_y = 0.70f;
    touch_stick_size = 0.17f;
    touch_opacity = 0.70f;
    touch_ui_mult = 1.0f;
}

void touch_layout_mirror(void) {
    for (int i = 0; i < TB_COUNT; i++) touch_buttons[i].x = 1.0f - touch_buttons[i].x;
    touch_stick_x = 1.0f - touch_stick_x;
}

void touch_layout_save(void) {
    FILE *f = fopen(LAYOUT_FILE, "w");
    if (!f) return;
    fprintf(f, "version 2\n");
    fprintf(f, "opacity %f\n", touch_opacity);
    fprintf(f, "ui_mult %f\n", touch_ui_mult);
    fprintf(f, "STICK %f %f %f 1\n", touch_stick_x, touch_stick_y, touch_stick_size);
    for (int i = 0; i < TB_COUNT; i++)
        fprintf(f, "%s %f %f %f %d\n", touch_buttons[i].name, touch_buttons[i].x, touch_buttons[i].y,
                touch_buttons[i].size, touch_buttons[i].visible);
    fclose(f);
}

void touch_layout_load(void) {
    touch_layout_reset();
    FILE *f = fopen(LAYOUT_FILE, "r");
    if (!f) return;
    char name[32];
    float a, b, c;
    int vis;
    char line[128];
    int version = 1;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "version %d", &vis) == 1) { version = vis; continue; }
        if (sscanf(line, "opacity %f", &a) == 1) { touch_opacity = a; continue; }
        if (sscanf(line, "ui_mult %f", &a) == 1) { if (a >= 0.5f && a <= 2.5f) touch_ui_mult = a; continue; }
        vis = 1;
        int n = sscanf(line, "%31s %f %f %f %d", name, &a, &b, &c, &vis);
        if (n >= 4) {
            if (!strcmp(name, "STICK")) { touch_stick_x = a; touch_stick_y = b; touch_stick_size = c; continue; }
            for (int i = 0; i < TB_COUNT; i++) {
                if (!strcmp(name, touch_buttons[i].name)) {
                    touch_buttons[i].x = a; touch_buttons[i].y = b; touch_buttons[i].size = c;
                    touch_buttons[i].visible = (n >= 5) ? (vis != 0) : 1;
                }
            }
        }
    }
    fclose(f);
    if (version < 2) touch_layout_reset();   // layout saved by an older design: use the new defaults
}

static void get_window_size(float *w, float *h) {
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    int iw = 1280, ih = 720;
    if (win) SDL_GetWindowSize(win, &iw, &ih);
    *w = (float)iw;
    *h = (float)ih;
}

static int hit_tool(float nx, float ny) {
    float w, h;
    get_window_size(&w, &h);
    float px = nx * w, py = ny * h;
    for (int i = 0; i < 2; i++) {
        float cx = touch_tool_x[i] * w, cy = touch_tool_y * h;
        float r = touch_tool_r * h * 1.2f;
        float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
        if (d <= r) return i;
    }
    return -1;
}

// Index of the button under the finger, -1 = stick, -2 = nothing.
static int hit_test(float nx, float ny, int include_stick, int include_hidden) {
    float w, h;
    get_window_size(&w, &h);
    float px = nx * w, py = ny * h;
    int best = -2;
    float best_d = 1e9f;
    for (int i = 0; i < TB_COUNT; i++) {
        if (!touch_buttons[i].visible && !include_hidden) continue;
        float cx = touch_buttons[i].x * w, cy = touch_buttons[i].y * h;
        float r = touch_buttons[i].size * h * 1.25f;
        float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
        if (d <= r && d < best_d) { best = i; best_d = d; }
    }
    if (best == -2 && include_stick) {
        float cx = touch_stick_x * w, cy = touch_stick_y * h;
        float r = touch_stick_size * h * 1.2f;
        float d = sqrtf((px - cx) * (px - cx) + (py - cy) * (py - cy));
        if (d <= r) best = -1;
    }
    return best;
}

static float clamp01(float v) { return v < 0.02f ? 0.02f : (v > 0.98f ? 0.98f : v); }

void touch_handle_event(const SDL_Event *e) {
    if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERMOTION && e->type != SDL_FINGERUP) return;

    float nx = e->tfinger.x, ny = e->tfinger.y;
    SDL_FingerID id = e->tfinger.fingerId;

    // --- toolbar (eye / menu): always works ---
    if (e->type == SDL_FINGERDOWN) {
        int t = hit_tool(nx, ny);
        if (t >= 0) { touch_tool_down[t] = 1; tool_finger[t] = id; return; }
    } else if (e->type == SDL_FINGERUP) {
        for (int t = 0; t < 2; t++) {
            if (touch_tool_down[t] && tool_finger[t] == id) {
                touch_tool_down[t] = 0;
                if (hit_tool(nx, ny) == t) pending_action = (t == 0) ? TA_EYE : TA_MENU;
                return;
            }
        }
    } else if (e->type == SDL_FINGERMOTION) {
        for (int t = 0; t < 2; t++) if (touch_tool_down[t] && tool_finger[t] == id) return;
    }

    // --- edit mode: drag buttons ---
    if (touch_edit) {
        if (e->type == SDL_FINGERDOWN) {
            if (point_in_ui(nx, ny)) return;
            int hit = hit_test(nx, ny, 1, 1);
            touch_selected = hit;
            if (hit != -2) { dragging = 1; drag_finger = id; }
        } else if (e->type == SDL_FINGERMOTION && dragging && id == drag_finger) {
            if (touch_selected >= 0) { touch_buttons[touch_selected].x = clamp01(nx); touch_buttons[touch_selected].y = clamp01(ny); }
            else if (touch_selected == -1) { touch_stick_x = clamp01(nx); touch_stick_y = clamp01(ny); }
        } else if (e->type == SDL_FINGERUP && id == drag_finger) {
            dragging = 0;
        }
        return;
    }

    if (!touch_enabled) return;

    if (e->type == SDL_FINGERDOWN) {
        if (point_in_ui(nx, ny)) return;           // the touch belongs to a Saturn window/menu
        int hit = hit_test(nx, ny, 0, 0);
        if (hit >= 0) {
            touch_buttons[hit].pressed = 1;
            touch_buttons[hit].finger = id;
        } else if (!touch_stick_active && nx < 0.5f) {
            touch_stick_active = 1;
            stick_finger = id;
            touch_stick_ox = nx; touch_stick_oy = ny;
            touch_stick_dx = touch_stick_dy = 0;
        }
    } else if (e->type == SDL_FINGERMOTION) {
        if (touch_stick_active && id == stick_finger) {
            float w, h;
            get_window_size(&w, &h);
            float rad = touch_stick_size * h;
            float dx = (nx - touch_stick_ox) * w / rad;
            float dy = (ny - touch_stick_oy) * h / rad;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 1.0f) { dx /= len; dy /= len; }
            touch_stick_dx = dx; touch_stick_dy = dy;
        }
    } else { // FINGERUP
        if (touch_stick_active && id == stick_finger) {
            touch_stick_active = 0;
            touch_stick_dx = touch_stick_dy = 0;
        }
        for (int i = 0; i < TB_COUNT; i++) {
            if (touch_buttons[i].pressed && touch_buttons[i].finger == id) touch_buttons[i].pressed = 0;
        }
    }
}

static void touch_init(void) {
    touch_layout_load();
}

static void touch_read(OSContPad *pad) {
    if (touch_edit || !touch_enabled) return;

    for (int i = 0; i < TB_COUNT; i++) {
        if (touch_buttons[i].pressed && touch_buttons[i].visible) pad->button |= touch_buttons[i].mask;
    }

    if (touch_stick_active) {
        float dx = touch_stick_dx, dy = touch_stick_dy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.08f) {
            int sx = (int)(dx * 80.0f);
            int sy = (int)(-dy * 80.0f);
            if (sx > 80) sx = 80; if (sx < -80) sx = -80;
            if (sy > 80) sy = 80; if (sy < -80) sy = -80;
            pad->stick_x = (s8)sx;
            pad->stick_y = (s8)sy;
        }
    }
}

static u32 touch_rawkey(void) {
    return VK_INVALID;
}

struct ControllerAPI controller_touch = {
    0x2000,
    touch_init,
    touch_read,
    touch_rawkey,
    NULL,
    NULL,
    NULL,
    NULL
};
