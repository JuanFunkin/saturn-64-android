#ifndef CONTROLLER_TOUCH_H
#define CONTROLLER_TOUCH_H

// Optional on-screen N64 gamepad (to walk Mario around while editing). Hidden by default.
// Positions are normalized (0..1 of the screen); sizes are radii as a fraction of the screen HEIGHT.

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TB_A, TB_B, TB_Z, TB_START,
    TB_CUP, TB_CDOWN, TB_CLEFT, TB_CRIGHT,
    TB_L, TB_R,
    TB_COUNT
};

typedef struct {
    const char *label;
    float x, y, size;
    unsigned int mask;
    int pressed;
    SDL_FingerID finger;
} TouchButton;

extern TouchButton touch_buttons[TB_COUNT];
extern float touch_stick_x, touch_stick_y, touch_stick_size;   // fixed stick base
extern float touch_stick_dx, touch_stick_dy;                   // -1..1 (screen axes, y down)
extern int   touch_stick_active;
extern int   touch_gamepad_visible;
extern float touch_opacity;

// returns 1 if the finger belongs to the gamepad (then the gesture layer must ignore it)
int  touch_handle_event(const SDL_Event *e);
void touch_release_all(void);
void touch_layout_reset(void);

struct ControllerAPI;
extern struct ControllerAPI controller_touch;

#ifdef __cplusplus
}
#endif
#endif
