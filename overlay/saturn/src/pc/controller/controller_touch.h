#ifndef CONTROLLER_TOUCH_H
#define CONTROLLER_TOUCH_H

// Touch controls for Saturn 64 Mobile (Android).
// Positions are normalized: x,y in [0,1] (button centre relative to the window) and size = radius as a
// fraction of the screen HEIGHT, so the layout fits any phone shape.

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
    const char *name;     // internal name (layout file)
    const char *label;    // text drawn on the button
    float x, y, size;
    unsigned int mask;    // N64 mask (A_BUTTON, B_BUTTON, ...)
    int pressed;
    SDL_FingerID finger;
    int visible;          // 0 = this button is disabled/hidden for good (user choice)
} TouchButton;

extern TouchButton touch_buttons[TB_COUNT];

// Virtual stick (floating: anchors where the finger lands, inside the left half of the screen)
extern float touch_stick_x, touch_stick_y, touch_stick_size;
extern float touch_stick_ox, touch_stick_oy;
extern float touch_stick_dx, touch_stick_dy;
extern int   touch_stick_active;

// Toolbar (always on screen): [0] = eye (hide/show buttons), [1] = menu
enum { TA_NONE = 0, TA_EYE = 1, TA_MENU = 2 };
extern float touch_tool_x[2], touch_tool_y, touch_tool_r;
extern int   touch_tool_down[2];
int touch_take_action(void);          // returns and clears the last toolbar tap

// hide state: 0 = everything visible, 1 = game buttons hidden (still active), 2 = everything hidden
extern int   touch_hide_state;
extern int   touch_enabled;           // the controls respond to touch (even when hidden)
extern int   touch_edit;              // edit-layout mode
extern int   touch_selected;          // selected in edit mode (-1 = stick, -2 = none)
extern int   touch_menu_open;         // quick menu drawer open
extern float touch_opacity;           // 0.1 - 1.0
extern float touch_ui_mult;           // Saturn UI size multiplier
extern int   touch_ui_capture;        // set by the UI: 1 if ImGui is using the touch

// Rectangles of the visible ImGui windows (normalized), so touches on them do not move Mario
void touch_ui_rects_begin(void);
void touch_ui_rect_add(float x0, float y0, float x1, float y1);

void touch_handle_event(const SDL_Event *e);
void touch_layout_save(void);
void touch_layout_load(void);
void touch_layout_reset(void);
void touch_layout_mirror(void);

struct ControllerAPI;
extern struct ControllerAPI controller_touch;

#ifdef __cplusplus
}
#endif

#endif
