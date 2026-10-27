#ifdef __ANDROID__

#include <SDL2/SDL.h>
#include <math.h>
#include <string.h>

#include <ultra64.h>

#include "controller_api.h"
#include "controller_touch.h"

TouchButton touch_buttons[TB_COUNT];
float touch_stick_x = 0.14f, touch_stick_y = 0.74f, touch_stick_size = 0.15f;
float touch_stick_dx, touch_stick_dy;
int   touch_stick_active = 0;
int   touch_gamepad_visible = 0;
float touch_opacity = 0.65f;

static SDL_FingerID stick_finger;

void touch_layout_reset(void) {
    static const struct { const char *label; float x, y, size; unsigned int mask; } def[TB_COUNT] = {
        { "A",     0.915f, 0.70f, 0.090f, A_BUTTON },
        { "B",     0.815f, 0.82f, 0.075f, B_BUTTON },
        { "Z",     0.715f, 0.74f, 0.068f, Z_TRIG },
        { "S",     0.500f, 0.92f, 0.050f, START_BUTTON },
        { "^",     0.885f, 0.27f, 0.050f, U_CBUTTONS },
        { "v",     0.885f, 0.47f, 0.050f, D_CBUTTONS },
        { "<",     0.815f, 0.37f, 0.050f, L_CBUTTONS },
        { ">",     0.955f, 0.37f, 0.050f, R_CBUTTONS },
        { "L",     0.060f, 0.45f, 0.060f, L_TRIG },
        { "R",     0.735f, 0.55f, 0.058f, R_TRIG },
    };
    for (int i = 0; i < TB_COUNT; i++) {
        touch_buttons[i].label = def[i].label;
        touch_buttons[i].x = def[i].x;
        touch_buttons[i].y = def[i].y;
        touch_buttons[i].size = def[i].size;
        touch_buttons[i].mask = def[i].mask;
        touch_buttons[i].pressed = 0;
    }
    touch_stick_x = 0.14f; touch_stick_y = 0.74f; touch_stick_size = 0.15f;
}

void touch_release_all(void) {
    for (int i = 0; i < TB_COUNT; i++) touch_buttons[i].pressed = 0;
    touch_stick_active = 0;
    touch_stick_dx = touch_stick_dy = 0;
}

static void window_size(float *w, float *h) {
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    int iw = 1280, ih = 720;
    if (win) SDL_GetWindowSize(win, &iw, &ih);
    *w = (float)iw; *h = (float)ih;
}

int touch_handle_event(const SDL_Event *e) {
    if (e->type != SDL_FINGERDOWN && e->type != SDL_FINGERMOTION && e->type != SDL_FINGERUP) return 0;
    if (!touch_gamepad_visible) return 0;

    float w, h;
    window_size(&w, &h);
    float px = e->tfinger.x * w, py = e->tfinger.y * h;
    SDL_FingerID id = e->tfinger.fingerId;

    if (e->type == SDL_FINGERDOWN) {
        // buttons (generous 1.25x hit radius)
        int best = -1;
        float best_d = 1e9f;
        for (int i = 0; i < TB_COUNT; i++) {
            float cx = touch_buttons[i].x * w, cy = touch_buttons[i].y * h;
            float d = hypotf(px - cx, py - cy);
            if (d <= touch_buttons[i].size * h * 1.25f && d < best_d) { best = i; best_d = d; }
        }
        if (best >= 0) {
            touch_buttons[best].pressed = 1;
            touch_buttons[best].finger = id;
            return 1;
        }
        float sx = touch_stick_x * w, sy = touch_stick_y * h;
        if (!touch_stick_active && hypotf(px - sx, py - sy) <= touch_stick_size * h * 1.4f) {
            touch_stick_active = 1;
            stick_finger = id;
            float r = touch_stick_size * h;
            touch_stick_dx = (px - sx) / r;
            touch_stick_dy = (py - sy) / r;
            float len = hypotf(touch_stick_dx, touch_stick_dy);
            if (len > 1.f) { touch_stick_dx /= len; touch_stick_dy /= len; }
            return 1;
        }
        return 0;
    }

    if (e->type == SDL_FINGERMOTION) {
        if (touch_stick_active && id == stick_finger) {
            float sx = touch_stick_x * w, sy = touch_stick_y * h, r = touch_stick_size * h;
            touch_stick_dx = (px - sx) / r;
            touch_stick_dy = (py - sy) / r;
            float len = hypotf(touch_stick_dx, touch_stick_dy);
            if (len > 1.f) { touch_stick_dx /= len; touch_stick_dy /= len; }
            return 1;
        }
        for (int i = 0; i < TB_COUNT; i++) if (touch_buttons[i].pressed && touch_buttons[i].finger == id) return 1;
        return 0;
    }

    // FINGERUP
    int claimed = 0;
    if (touch_stick_active && id == stick_finger) {
        touch_stick_active = 0;
        touch_stick_dx = touch_stick_dy = 0;
        claimed = 1;
    }
    for (int i = 0; i < TB_COUNT; i++) {
        if (touch_buttons[i].pressed && touch_buttons[i].finger == id) { touch_buttons[i].pressed = 0; claimed = 1; }
    }
    return claimed;
}

static void touch_init(void) {
    touch_layout_reset();
}

static void touch_read(OSContPad *pad) {
    if (!touch_gamepad_visible) return;
    for (int i = 0; i < TB_COUNT; i++)
        if (touch_buttons[i].pressed) pad->button |= touch_buttons[i].mask;
    if (touch_stick_active) {
        float dx = touch_stick_dx, dy = touch_stick_dy;
        if (hypotf(dx, dy) > 0.12f) {
            int sx = (int)(dx * 80.0f), sy = (int)(-dy * 80.0f);
            if (sx > 80) sx = 80;
            if (sx < -80) sx = -80;
            if (sy > 80) sy = 80;
            if (sy < -80) sy = -80;
            pad->stick_x = (s8)sx;
            pad->stick_y = (s8)sy;
        }
    }
}

static u32 touch_rawkey(void) { return VK_INVALID; }

struct ControllerAPI controller_touch = {
    0x2000,
    touch_init,
    touch_read,
    touch_rawkey,
    NULL, NULL, NULL, NULL
};

#endif
