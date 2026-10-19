#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <ultra64.h>

#include "controller_api.h"
#include "controller_touch.h"

#define LAYOUT_FILE "touch_layout.cfg"

TouchButton touch_buttons[TB_COUNT];
float touch_stick_x, touch_stick_y, touch_stick_size;
float touch_stick_ox, touch_stick_oy;
float touch_stick_dx, touch_stick_dy;
int   touch_stick_active = 0;

int   touch_visible = 1;
int   touch_enabled = 1;
int   touch_edit = 0;
int   touch_selected = -2;
float touch_opacity = 0.55f;
float touch_ui_mult = 1.0f;
int   touch_ui_capture = 0;

static SDL_FingerID stick_finger = 0;
static SDL_FingerID drag_finger = 0;
static int dragging = 0;           // 1 si se esta arrastrando touch_selected en modo edicion

void touch_layout_reset(void) {
    static const struct { const char *name, *label; float x, y, size; unsigned int mask; } def[TB_COUNT] = {
        { "A",      "A",  0.90f, 0.74f, 0.100f, A_BUTTON },
        { "B",      "B",  0.79f, 0.86f, 0.085f, B_BUTTON },
        { "Z",      "Z",  0.30f, 0.86f, 0.085f, Z_TRIG },
        { "START",  "St", 0.50f, 0.09f, 0.060f, START_BUTTON },
        { "CUP",    "C^", 0.88f, 0.28f, 0.060f, U_CBUTTONS },
        { "CDOWN",  "Cv", 0.88f, 0.52f, 0.060f, D_CBUTTONS },
        { "CLEFT",  "C<", 0.78f, 0.40f, 0.060f, L_CBUTTONS },
        { "CRIGHT", "C>", 0.97f, 0.40f, 0.060f, R_CBUTTONS },
        { "L",      "L",  0.07f, 0.10f, 0.070f, L_TRIG },
        { "R",      "R",  0.93f, 0.10f, 0.070f, R_TRIG },
    };
    for (int i = 0; i < TB_COUNT; i++) {
        touch_buttons[i].name = def[i].name;
        touch_buttons[i].label = def[i].label;
        touch_buttons[i].x = def[i].x;
        touch_buttons[i].y = def[i].y;
        touch_buttons[i].size = def[i].size;
        touch_buttons[i].mask = def[i].mask;
        touch_buttons[i].pressed = 0;
    }
    touch_stick_x = 0.14f;
    touch_stick_y = 0.70f;
    touch_stick_size = 0.17f;
    touch_opacity = 0.55f;
    touch_ui_mult = 1.0f;
}

void touch_layout_save(void) {
    FILE *f = fopen(LAYOUT_FILE, "w");
    if (!f) return;
    fprintf(f, "opacity %f\n", touch_opacity);
    fprintf(f, "ui_mult %f\n", touch_ui_mult);
    fprintf(f, "STICK %f %f %f\n", touch_stick_x, touch_stick_y, touch_stick_size);
    for (int i = 0; i < TB_COUNT; i++)
        fprintf(f, "%s %f %f %f\n", touch_buttons[i].name, touch_buttons[i].x, touch_buttons[i].y, touch_buttons[i].size);
    fclose(f);
}

void touch_layout_load(void) {
    touch_layout_reset();
    FILE *f = fopen(LAYOUT_FILE, "r");
    if (!f) return;
    char name[32];
    float a, b, c;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "opacity %f", &a) == 1) { touch_opacity = a; continue; }
        if (sscanf(line, "ui_mult %f", &a) == 1) { if (a >= 0.5f && a <= 2.5f) touch_ui_mult = a; continue; }
        if (sscanf(line, "%31s %f %f %f", name, &a, &b, &c) == 4) {
            if (!strcmp(name, "STICK")) { touch_stick_x = a; touch_stick_y = b; touch_stick_size = c; continue; }
            for (int i = 0; i < TB_COUNT; i++) {
                if (!strcmp(name, touch_buttons[i].name)) {
                    touch_buttons[i].x = a; touch_buttons[i].y = b; touch_buttons[i].size = c;
                }
            }
        }
    }
    fclose(f);
}

static void get_window_size(float *w, float *h) {
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    int iw = 1280, ih = 720;
    if (win) SDL_GetWindowSize(win, &iw, &ih);
    *w = (float)iw;
    *h = (float)ih;
}

// Devuelve el indice del boton bajo (px,py) en pixeles, -1 si es el stick, -2 si nada.
static int hit_test(float nx, float ny, int include_stick) {
    float w, h;
    get_window_size(&w, &h);
    float px = nx * w, py = ny * h;
    int best = -2;
    float best_d = 1e9f;
    for (int i = 0; i < TB_COUNT; i++) {
        float cx = touch_buttons[i].x * w, cy = touch_buttons[i].y * h;
        float r = touch_buttons[i].size * h * 1.25f; // un poco de margen
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

void touch_handle_event(const SDL_Event *e) {
    if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERMOTION && e->type != SDL_FINGERUP) return;

    float nx = e->tfinger.x, ny = e->tfinger.y;
    SDL_FingerID id = e->tfinger.fingerId;

    if (touch_edit) {
        if (e->type == SDL_FINGERDOWN && !touch_ui_capture) {
            int hit = hit_test(nx, ny, 1);
            touch_selected = hit;
            if (hit != -2) { dragging = 1; drag_finger = id; }
        } else if (e->type == SDL_FINGERMOTION && dragging && id == drag_finger) {
            if (touch_selected >= 0) { touch_buttons[touch_selected].x = nx; touch_buttons[touch_selected].y = ny; }
            else if (touch_selected == -1) { touch_stick_x = nx; touch_stick_y = ny; }
        } else if (e->type == SDL_FINGERUP && id == drag_finger) {
            dragging = 0;
        }
        return;
    }

    if (!touch_enabled) return;

    if (e->type == SDL_FINGERDOWN) {
        if (touch_ui_capture) return;
        int hit = hit_test(nx, ny, 0);
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
        if (touch_buttons[i].pressed) pad->button |= touch_buttons[i].mask;
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
