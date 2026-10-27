// Saturn Studio for Android: touch input layer + mobile HUD.
//
//  * 1 finger on the 3D view   -> move the camera (left-drag = pan)
//  * 2 fingers on the 3D view  -> rotate (right-drag) + pinch to zoom (mouse wheel)
//  * long press on the 3D view -> right click (select a Mario / open its menu)
//  * tap on the 3D view        -> click (spawns a Mario only when the "Tap" tool is on)
//  * 1 finger on a panel       -> mouse (press immediately: sliders, drags, docking all work)
//  * 2 fingers on a panel      -> scroll that panel
//
// Finger events are collected by saturn_mobile_handle_finger() and turned into Dear ImGui mouse events once per
// frame (saturn_mobile_begin_frame), because ImGui needs the hover state of the previous frame.
#ifdef __ANDROID__

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <filesystem>

#include "saturn/touch/saturn_touch_api.h"
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"
#include "saturn/imgui/saturn_imgui.h"
#include "saturn/libs/imgui/imgui_impl_opengl3.h"
#include "saturn/imgui/icons/IconsForkAwesome.h"

extern "C" {
#include "pc/controller/controller_touch.h"
#include "pc/android_support.h"
}

extern std::map<std::string, bool> visible_windows; // saturn_imgui.cpp

// ---------------------------------------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------------------------------------

#define SETTINGS_FILE "mobile_settings.cfg"

static struct {
    float ui_mult = 1.0f;       // UI size (applied at the next start)
    float cam_sens = 1.6f;      // camera drag multiplier
    int   tap_tool = 0;         // tap on the game view spawns / selects Marios
    int   gamepad = 0;          // on-screen N64 gamepad visible
    float opacity = 0.65f;      // gamepad opacity
    int   toolbar_collapsed = 0;
} cfg;

static bool cfg_loaded = false;

static void settings_load() {
    cfg_loaded = true;
    FILE *f = fopen(SETTINGS_FILE, "r");
    if (!f) return;
    char line[128];
    float a;
    int i;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "ui_mult %f", &a) == 1) { if (a >= 0.6f && a <= 1.8f) cfg.ui_mult = a; }
        else if (sscanf(line, "cam_sens %f", &a) == 1) { if (a >= 0.2f && a <= 5.f) cfg.cam_sens = a; }
        else if (sscanf(line, "tap_tool %d", &i) == 1) cfg.tap_tool = i != 0;
        else if (sscanf(line, "gamepad %d", &i) == 1) cfg.gamepad = i != 0;
        else if (sscanf(line, "opacity %f", &a) == 1) { if (a >= 0.1f && a <= 1.f) cfg.opacity = a; }
        else if (sscanf(line, "toolbar_collapsed %d", &i) == 1) cfg.toolbar_collapsed = i != 0;
    }
    fclose(f);
}

static void settings_save() {
    FILE *f = fopen(SETTINGS_FILE, "w");
    if (!f) return;
    fprintf(f, "ui_mult %f\ncam_sens %f\ntap_tool %d\ngamepad %d\nopacity %f\ntoolbar_collapsed %d\n",
            cfg.ui_mult, cfg.cam_sens, cfg.tap_tool, cfg.gamepad, cfg.opacity, cfg.toolbar_collapsed);
    fclose(f);
}

static void settings_apply_to_gamepad() {
    touch_gamepad_visible = cfg.gamepad;
    touch_opacity = cfg.opacity;
    if (!cfg.gamepad) touch_release_all();
}

// ---------------------------------------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------------------------------------

static float ui_scale = 0.f;
static float ui_scale_base = 0.f;   // scale for ui_mult = 1

extern "C" float saturn_mobile_ui_scale(void) {
    if (ui_scale > 0.f) return ui_scale;
    if (!cfg_loaded) { settings_load(); }
    int w = 0, h = 0;
    SDL_Window *win = SDL_GL_GetCurrentWindow();
    if (win) SDL_GetWindowSize(win, &w, &h);
    if (w <= 0 || h <= 0) {
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0) { w = dm.w; h = dm.h; }
    }
    if (w <= 0 || h <= 0) return 2.0f; // do not cache: the window is not ready yet
    int shortSide = w < h ? w : h;
    // Keep the desktop look: the layout is shown at ~640 logical pixels of height and scaled up to the screen.
    float base = (float)shortSide / 640.0f;
    if (base < 1.0f) base = 1.0f;
    if (base > 4.0f) base = 4.0f;
    ui_scale_base = base;
    ui_scale = base * cfg.ui_mult;
    if (ui_scale < 1.0f) ui_scale = 1.0f;
    settings_apply_to_gamepad();
    return ui_scale;
}

extern "C" float saturn_touch_camera_sensitivity(void) { return cfg.cam_sens; }
extern "C" int saturn_touch_tap_enabled(void) { return cfg.tap_tool; }

void saturn_mobile_apply_style(ImGuiStyle *style) {
    // Same look as the desktop version; only the things that matter for fingers get a bit more room.
    style->FramePadding = ImVec2(6, 5);
    style->ItemSpacing = ImVec2(8, 6);
    style->ScrollbarSize = 20.0f;
    style->GrabMinSize = 20.0f;
    style->TouchExtraPadding = ImVec2(4, 4);
    style->DockingSeparatorSize = 8.0f;
    style->WindowMenuButtonPosition = ImGuiDir_None;
}

// Fonts are rasterized at (logical size * UI scale) so they stay sharp, then drawn at 1/UI scale.
static const float FONT_PX = 14.0f;
static bool fonts_rebuild_pending = false;

void saturn_mobile_build_fonts() {
    ImGuiIO &io = ImGui::GetIO();
    const float S = saturn_mobile_ui_scale();
    ImFontConfig defaultConfig;
    defaultConfig.SizePixels = FONT_PX * S;
    io.Fonts->AddFontDefault(&defaultConfig);

    ImFontConfig symbolConfig;
    symbolConfig.MergeMode = true;
    symbolConfig.SizePixels = FONT_PX * S;
    symbolConfig.GlyphMinAdvanceX = FONT_PX * S;
    static const ImWchar icon_ranges[] = { ICON_MIN_FK, ICON_MAX_FK, 0 };
    io.Fonts->AddFontFromFileTTF("fonts/forkawesome-webfont.ttf", symbolConfig.SizePixels, &symbolConfig, icon_ranges);
    io.FontGlobalScale = 1.0f / S;
}

// ---------------------------------------------------------------------------------------------------------
// Finger tracking
// ---------------------------------------------------------------------------------------------------------

struct Finger {
    SDL_FingerID id;
    ImVec2 pos;     // logical (ImGui) pixels
    ImVec2 start;
    Uint32 t0;
};

static std::vector<Finger> fingers;

extern "C" void saturn_mobile_handle_finger(const SDL_Event *e) {
    if (!ImGui::GetCurrentContext()) return; // ImGui is created after the splash screen
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 p(e->tfinger.x * io.DisplaySize.x, e->tfinger.y * io.DisplaySize.y);
    SDL_FingerID id = e->tfinger.fingerId;
    if (e->type == SDL_FINGERDOWN) {
        for (auto &f : fingers) if (f.id == id) { f.pos = p; return; }
        if (fingers.size() >= 4) return;
        fingers.push_back({ id, p, p, SDL_GetTicks() });
    } else if (e->type == SDL_FINGERMOTION) {
        for (auto &f : fingers) if (f.id == id) f.pos = p;
    } else if (e->type == SDL_FINGERUP) {
        for (size_t i = 0; i < fingers.size(); i++) {
            if (fingers[i].id == id) {
                fingers[i].pos = p;
                fingers.erase(fingers.begin() + i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------
// UI hit test
// ---------------------------------------------------------------------------------------------------------

static bool in_rect(ImVec2 p, ImVec2 a, ImVec2 b) { return p.x >= a.x && p.y >= a.y && p.x <= b.x && p.y <= b.y; }

// True when the point is over a Dear ImGui window other than the 3D view ("Game").
static bool over_ui(ImVec2 p) {
    ImGuiContext *g = ImGui::GetCurrentContext();
    if (!g) return false;
    for (int i = 0; i < g->Windows.Size; i++) {
        ImGuiWindow *w = g->Windows[i];
        if (!w->Active || w->Hidden) continue;
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoMouseInputs)) continue;
        const char *n = w->Name;
        if (!strcmp(n, "Game") || !strcmp(n, "Dockspace")) continue;
        if (!strncmp(n, "##Tooltip", 9) || !strncmp(n, "##Navigate", 10) || !strncmp(n, "##NavWindowing", 14)) continue;
        if (!strncmp(n, "WindowOverViewport", 18) || !strncmp(n, "##Background", 12)) continue;
        if (w->DockNode && !w->DockTabIsVisible) continue;
        if (in_rect(p, w->Pos, ImVec2(w->Pos.x + w->Size.x, w->Pos.y + w->Size.y))) return true;
    }
    // The strip above the game image (tab / title bar of the Game window) belongs to the UI as well.
    if (game_viewport[2] > 0 && !in_rect(p, ImVec2(game_viewport[0], game_viewport[1]),
            ImVec2(game_viewport[0] + game_viewport[2], game_viewport[1] + game_viewport[3]))) return true;
    return false;
}

// ---------------------------------------------------------------------------------------------------------
// Gesture state machine (one step per frame)
// ---------------------------------------------------------------------------------------------------------

enum Mode {
    M_IDLE,
    M_UI_POINTER,     // one finger on a panel = left mouse button
    M_UI_SCROLL,      // two fingers on a panel = wheel
    M_CAM_WAIT,       // finger on the 3D view, not decided yet
    M_CAM_PAN,        // left button held, camera moves
    M_CAM_ROTATE,     // right button held (two fingers) + pinch zoom
    M_RELEASE_LEFT,   // synthetic click: release next frame
    M_RELEASE_RIGHT,
    M_WAIT_UP         // ignore everything until all fingers are up
};

static Mode mode = M_IDLE;
static int wait_frames = 0;
static float last_pinch = 0.f;
static ImVec2 last_mid(0, 0);
static bool leave_pending = false;

static const float SLOP = 14.0f;          // physical pixels before a touch becomes a drag
static const Uint32 LONG_PRESS_MS = 450;
static const Uint32 TAP_MS = 350;

static void mouse_pos(ImVec2 p) { ImGui::GetIO().AddMousePosEvent(p.x, p.y); }
static void mouse_btn(int b, bool d) { ImGui::GetIO().AddMouseButtonEvent(b, d); }

static ImVec2 midpoint() {
    return ImVec2((fingers[0].pos.x + fingers[1].pos.x) * 0.5f, (fingers[0].pos.y + fingers[1].pos.y) * 0.5f);
}
static float pinch_dist() {
    return hypotf(fingers[0].pos.x - fingers[1].pos.x, fingers[0].pos.y - fingers[1].pos.y);
}

extern "C" void saturn_mobile_begin_frame(void) {
    ImGuiIO &io = ImGui::GetIO();
    if (fonts_rebuild_pending) { // UI size changed from the menu: rebuild the font atlas at the new size
        fonts_rebuild_pending = false;
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        io.Fonts->Clear();
        saturn_mobile_build_fonts();
        io.Fonts->Build();
        ImGui_ImplOpenGL3_CreateFontsTexture();
    }
    const float S = saturn_mobile_ui_scale();
    const size_t n = fingers.size();
    const float slop = SLOP / S;

    switch (mode) {
    case M_IDLE:
        if (n >= 1) {
            leave_pending = false;
            ImVec2 p = fingers[0].pos;
            if (over_ui(p)) {
                mouse_pos(p);
                mouse_btn(0, true);
                mode = (n >= 2) ? M_WAIT_UP : M_UI_POINTER;
            } else {
                mouse_pos(p);   // hover first, so the game view gets focus
                wait_frames = 0;
                mode = M_CAM_WAIT;
            }
        } else if (leave_pending) {
            leave_pending = false;
            mouse_pos(ImVec2(-FLT_MAX, -FLT_MAX));
        }
        break;

    case M_UI_POINTER:
        if (n == 0) {
            mouse_btn(0, false);
            mode = M_IDLE;
            leave_pending = true;
        } else if (n >= 2) {
            // switch to scrolling without triggering a click: leave first, then release
            mouse_pos(ImVec2(-FLT_MAX, -FLT_MAX));
            mouse_btn(0, false);
            last_mid = midpoint();
            mouse_pos(last_mid);
            mode = M_UI_SCROLL;
        } else {
            mouse_pos(fingers[0].pos);
        }
        break;

    case M_UI_SCROLL:
        if (n < 2) {
            mouse_pos(ImVec2(-FLT_MAX, -FLT_MAX));
            mode = (n == 0) ? M_IDLE : M_WAIT_UP;
            leave_pending = true;
        } else {
            ImVec2 mid = midpoint();
            mouse_pos(mid);
            float dy = mid.y - last_mid.y;
            if (fabsf(dy) > 0.01f) io.AddMouseWheelEvent(0.f, dy / 70.0f);
            last_mid = mid;
        }
        break;

    case M_CAM_WAIT: {
        wait_frames++;
        if (n == 0) {
            // quick tap: click at the last position (spawns only when the tap tool is on)
            mouse_btn(0, true);
            mode = M_RELEASE_LEFT;
        } else if (n >= 2) {
            last_mid = midpoint();
            last_pinch = pinch_dist();
            mouse_pos(last_mid);
            mouse_btn(1, true);
            mode = M_CAM_ROTATE;
        } else {
            Finger &f = fingers[0];
            float moved = hypotf(f.pos.x - f.start.x, f.pos.y - f.start.y);
            Uint32 held = SDL_GetTicks() - f.t0;
            if (moved > slop && wait_frames >= 4) {
                mouse_pos(f.start);
                mouse_btn(0, true);
                mouse_pos(f.pos);
                mode = M_CAM_PAN;
            } else if (moved <= slop && held >= LONG_PRESS_MS) {
                mouse_pos(f.start);
                mouse_btn(1, true);   // right click: select a Mario
                mode = M_RELEASE_RIGHT;
            }
        }
        break;
    }

    case M_CAM_PAN:
        if (n == 0) {
            mouse_btn(0, false);
            mode = M_IDLE;
            leave_pending = true;
        } else if (n >= 2) {
            mouse_btn(0, false);
            last_mid = midpoint();
            last_pinch = pinch_dist();
            mouse_pos(last_mid);
            mouse_btn(1, true);
            mode = M_CAM_ROTATE;
        } else {
            mouse_pos(fingers[0].pos);
        }
        break;

    case M_CAM_ROTATE:
        if (n < 2) {
            mouse_btn(1, false);
            mode = (n == 0) ? M_IDLE : M_WAIT_UP;
            leave_pending = true;
        } else {
            ImVec2 mid = midpoint();
            mouse_pos(mid);
            float d = pinch_dist();
            float dd = d - last_pinch;
            if (fabsf(dd) > 0.5f / S) io.AddMouseWheelEvent(0.f, dd / 30.0f);
            last_pinch = d;
            last_mid = mid;
        }
        break;

    case M_RELEASE_LEFT:
        mouse_btn(0, false);
        mode = (n == 0) ? M_IDLE : M_WAIT_UP;
        leave_pending = true;
        break;

    case M_RELEASE_RIGHT:
        mouse_btn(1, false);
        mode = (n == 0) ? M_IDLE : M_WAIT_UP;
        leave_pending = true;
        break;

    case M_WAIT_UP:
        if (n == 0) { mode = M_IDLE; leave_pending = true; }
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------
// HUD: toolbar + panels menu + options + on-screen gamepad
// ---------------------------------------------------------------------------------------------------------

static std::string window_label(const std::string &key) {
    size_t p = key.find("###");
    return p == std::string::npos ? key : key.substr(0, p);
}

static void draw_toolbar() {
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 6, 6), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::SetNextWindowBgAlpha(0.55f);
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize |
                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::Begin("##mobile_toolbar", nullptr, fl)) {
        bool changed = false;
        // three-bar menu button
        if (ImGui::Button(ICON_FK_BARS "###mobile_menu_btn", ImVec2(34, 30))) ImGui::OpenPopup("##mobile_menu");

        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 6, 44), ImGuiCond_Always, ImVec2(1, 0));
        if (ImGui::BeginPopup("##mobile_menu")) {
            ImGui::TextDisabled("Windows");
            for (auto &entry : visible_windows) {
                std::string label = window_label(entry.first) + "###pw_" + entry.first;
                ImGui::Checkbox(label.c_str(), &entry.second);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Touch");
            bool tap = cfg.tap_tool != 0;
            if (ImGui::Checkbox("Tap to spawn / select Marios", &tap)) { cfg.tap_tool = tap; changed = true; }
            bool pad = cfg.gamepad != 0;
            if (ImGui::Checkbox("On-screen gamepad", &pad)) { cfg.gamepad = pad; settings_apply_to_gamepad(); changed = true; }
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("Camera speed", &cfg.cam_sens, 0.3f, 4.0f, "%.1f")) changed = true;
            ImGui::SetNextItemWidth(180);
            if (ImGui::SliderFloat("Gamepad opacity", &cfg.opacity, 0.15f, 1.0f, "%.2f")) { settings_apply_to_gamepad(); changed = true; }
            ImGui::SetNextItemWidth(180);
            ImGui::SliderFloat("HUD size", &cfg.ui_mult, 0.7f, 1.6f, "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) { // applied when the finger is lifted
                ui_scale = ui_scale_base * cfg.ui_mult;
                if (ui_scale < 1.0f) ui_scale = 1.0f;
                fonts_rebuild_pending = true;
                changed = true;
            }
            if (ImGui::Button("Reset gamepad")) touch_layout_reset();
            ImGui::SameLine();
            if (ImGui::Button("Reset windows")) {
                std::error_code ec;
                std::filesystem::remove("imgui.ini", ec);
                std::filesystem::remove("windows.bin", ec);
                android_toast("Layout reset. Close and reopen the app.");
            }
            ImGui::Separator();
            ImGui::TextDisabled("1 finger: move camera\n2 fingers: rotate + pinch zoom\nLong press: select Mario\n2 fingers on a window: scroll");
            ImGui::EndPopup();
        }
        if (changed) settings_save();
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

static void draw_gamepad() {
    if (!touch_gamepad_visible) return;
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    const float a = touch_opacity;

    // stick
    {
        ImVec2 c(touch_stick_x * W, touch_stick_y * H);
        float r = touch_stick_size * H;
        dl->AddCircleFilled(c, r, ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.12f, 0.45f * a)), 48);
        dl->AddCircle(c, r, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.7f * a)), 48, 2.0f);
        ImVec2 k(c.x + touch_stick_dx * r * 0.7f, c.y + touch_stick_dy * r * 0.7f);
        dl->AddCircleFilled(k, r * 0.42f, ImGui::GetColorU32(ImVec4(0.85f, 0.85f, 0.9f, a)), 32);
    }
    // buttons (N64 colours)
    static const ImVec4 colors[TB_COUNT] = {
        ImVec4(0.20f, 0.45f, 0.95f, 1), ImVec4(0.20f, 0.75f, 0.35f, 1), ImVec4(0.6f, 0.6f, 0.65f, 1), ImVec4(0.90f, 0.25f, 0.25f, 1),
        ImVec4(0.95f, 0.80f, 0.20f, 1), ImVec4(0.95f, 0.80f, 0.20f, 1), ImVec4(0.95f, 0.80f, 0.20f, 1), ImVec4(0.95f, 0.80f, 0.20f, 1),
        ImVec4(0.6f, 0.6f, 0.65f, 1), ImVec4(0.6f, 0.6f, 0.65f, 1),
    };
    for (int i = 0; i < TB_COUNT; i++) {
        ImVec2 c(touch_buttons[i].x * W, touch_buttons[i].y * H);
        float r = touch_buttons[i].size * H;
        ImVec4 col = colors[i];
        col.w = (touch_buttons[i].pressed ? 1.0f : 0.8f) * a;
        if (touch_buttons[i].pressed) { col.x = col.x * 0.7f + 0.3f; col.y = col.y * 0.7f + 0.3f; col.z = col.z * 0.7f + 0.3f; }
        dl->AddCircleFilled(c, r, ImGui::GetColorU32(col), 32);
        dl->AddCircle(c, r, ImGui::GetColorU32(ImVec4(1, 1, 1, 0.8f * a)), 32, 2.0f);
        ImVec2 ts = ImGui::CalcTextSize(touch_buttons[i].label);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), ImGui::GetColorU32(ImVec4(1, 1, 1, std::min(1.f, a + 0.25f))), touch_buttons[i].label);
    }
}

extern "C" void saturn_mobile_draw(void) {
    ImGuiIO &io = ImGui::GetIO();
    draw_toolbar();
    draw_gamepad();
    // soft keyboard
    if (io.WantTextInput) { if (!SDL_IsTextInputActive()) SDL_StartTextInput(); }
    else if (SDL_IsTextInputActive()) SDL_StopTextInput();
}

#endif
