#ifndef SATURN_TOUCH_API_H
#define SATURN_TOUCH_API_H

// Touch layer for Saturn Studio on Android (see saturn_mobile_ui.cpp).
#include <SDL2/SDL_events.h>

#ifdef __cplusplus
extern "C" {
#endif

float saturn_mobile_ui_scale(void);          // physical pixels per ImGui logical pixel (fixed at startup)
void  saturn_mobile_handle_finger(const SDL_Event *e); // called for every SDL_FINGER* event not claimed by the gamepad
void  saturn_mobile_begin_frame(void);       // before ImGui::NewFrame(): turns finger state into mouse events
void  saturn_mobile_draw(void);              // before ImGui::Render(): toolbar + on-screen gamepad
float saturn_touch_camera_sensitivity(void); // multiplier applied to camera drag deltas
int   saturn_touch_tap_enabled(void);        // 1 = a tap on the game view spawns / selects Marios

#ifdef __cplusplus
}
struct ImGuiStyle;
void saturn_mobile_apply_style(ImGuiStyle *style);
void saturn_mobile_build_fonts();
#endif

#endif
