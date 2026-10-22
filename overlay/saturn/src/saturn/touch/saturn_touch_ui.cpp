// Touch HUD for Saturn 64 Mobile: game buttons, stick, toolbar (eye + menu), quick menu drawer and the
// layout editor. Called once per frame from saturn_imgui_update(), right before ImGui::Render().
// Everything is drawn with ImGui's foreground draw list, so hiding the controls (or Saturn's UI) leaves the
// game image clean for screenshots and videos.

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "saturn/libs/imgui/imgui.h"
#include "saturn/imgui/saturn_imgui_chroma.h"   // schroma_imgui_init()

extern "C" {
#include "pc/controller/controller_touch.h"
}

extern bool showMenu, showStatusBars, windowStats, windowCcEditor, windowAnimPlayer, windowSettings;
extern bool autoChroma, k_popout_open;

static ImU32 rgba(float r, float g, float b, float a) {
    if (r > 255.f) r = 255.f; if (g > 255.f) g = 255.f; if (b > 255.f) b = 255.f;
    if (a > 1.f) a = 1.f; if (a < 0.f) a = 0.f;
    return IM_COL32((int)r, (int)g, (int)b, (int)(a * 255.f));
}

struct Rgb { float r, g, b; };

static Rgb button_color(int i) {
    switch (i) {
        case TB_A:      return { 70.f, 120.f, 255.f };
        case TB_B:      return { 70.f, 200.f, 100.f };
        case TB_Z:      return { 165.f, 165.f, 178.f };
        case TB_START:  return { 235.f, 70.f, 70.f };
        case TB_CUP: case TB_CDOWN: case TB_CLEFT: case TB_CRIGHT: return { 255.f, 214.f, 64.f };
        default:        return { 120.f, 120.f, 140.f };   // L, R
    }
}

static void draw_arrow(ImDrawList *dl, ImVec2 c, float s, float dx, float dy, ImU32 col) {
    float px = -dy, py = dx;
    ImVec2 tip(c.x + dx * s, c.y + dy * s);
    ImVec2 b1(c.x - dx * s * 0.7f + px * s, c.y - dy * s * 0.7f + py * s);
    ImVec2 b2(c.x - dx * s * 0.7f - px * s, c.y - dy * s * 0.7f - py * s);
    dl->AddTriangleFilled(tip, b1, b2, col);
}

static void draw_game_button(ImDrawList *dl, ImVec2 c, float r, int idx, bool pressed, bool selected, bool disabled) {
    float a = touch_opacity;
    if (disabled) a *= 0.30f;
    Rgb k = button_color(idx);
    float rr = pressed ? r * 0.94f : r;
    float lift = pressed ? 0.f : rr * 0.07f;

    dl->AddCircleFilled(ImVec2(c.x, c.y + rr * 0.10f), rr, rgba(0, 0, 0, 0.35f * a), 40);                 // shadow
    dl->AddCircleFilled(c, rr, rgba(k.r * 0.55f, k.g * 0.55f, k.b * 0.55f, a), 40);                       // dark rim
    float boost = pressed ? 45.f : 0.f;
    dl->AddCircleFilled(ImVec2(c.x, c.y - lift), rr * 0.88f, rgba(k.r + boost, k.g + boost, k.b + boost, a), 40); // face
    dl->AddCircleFilled(ImVec2(c.x - rr * 0.24f, c.y - rr * 0.32f - lift), rr * 0.30f, rgba(255, 255, 255, 0.20f * a), 24); // shine
    dl->AddCircle(c, rr, selected ? rgba(255, 210, 40, 1.f) : rgba(255, 255, 255, 0.55f * a), 40, selected ? 5.f : 2.f);

    ImU32 ink = (idx == TB_Z || idx == TB_CUP || idx == TB_CDOWN || idx == TB_CLEFT || idx == TB_CRIGHT)
                    ? rgba(40, 40, 55, a + 0.25f) : rgba(255, 255, 255, a + 0.25f);
    ImVec2 fc(c.x, c.y - lift);
    float s = rr * 0.40f;
    if (idx == TB_CUP) draw_arrow(dl, fc, s, 0.f, -1.f, ink);
    else if (idx == TB_CDOWN) draw_arrow(dl, fc, s, 0.f, 1.f, ink);
    else if (idx == TB_CLEFT) draw_arrow(dl, fc, s, -1.f, 0.f, ink);
    else if (idx == TB_CRIGHT) draw_arrow(dl, fc, s, 1.f, 0.f, ink);
    else {
        const char *label = touch_buttons[idx].label;
        ImFont *font = ImGui::GetFont();
        float fs = rr * (idx == TB_START ? 0.46f : 0.95f);
        ImVec2 ts = font->CalcTextSizeA(fs, 100000.f, 0.f, label);
        ImVec2 p(fc.x - ts.x * 0.5f, fc.y - ts.y * 0.5f);
        dl->AddText(font, fs, ImVec2(p.x + 1.5f, p.y + 1.5f), rgba(0, 0, 0, 0.5f * a), label);
        dl->AddText(font, fs, p, ink, label);
    }
}

static void draw_stick(ImDrawList *dl, ImVec2 ds, bool selected) {
    float a = touch_opacity;
    float sr = touch_stick_size * ds.y;
    ImVec2 base = touch_stick_active ? ImVec2(touch_stick_ox * ds.x, touch_stick_oy * ds.y)
                                     : ImVec2(touch_stick_x * ds.x, touch_stick_y * ds.y);
    dl->AddCircleFilled(base, sr, rgba(0, 0, 0, 0.28f * a), 48);
    dl->AddCircle(base, sr, selected ? rgba(255, 210, 40, 1.f) : rgba(255, 255, 255, 0.60f * a), 48, selected ? 5.f : 3.f);
    dl->AddCircle(base, sr * 0.55f, rgba(255, 255, 255, 0.18f * a), 48, 2.f);
    ImVec2 knob(base.x + touch_stick_dx * sr * 0.85f, base.y + touch_stick_dy * sr * 0.85f);
    dl->AddCircleFilled(ImVec2(knob.x, knob.y + sr * 0.04f), sr * 0.42f, rgba(0, 0, 0, 0.35f * a), 32);
    dl->AddCircleFilled(knob, sr * 0.42f, rgba(235, 235, 245, 0.80f * a), 32);
    dl->AddCircleFilled(ImVec2(knob.x - sr * 0.10f, knob.y - sr * 0.13f), sr * 0.14f, rgba(255, 255, 255, 0.55f * a), 20);
}

static void draw_toolbar(ImDrawList *dl, ImVec2 ds) {
    for (int i = 0; i < 2; i++) {
        ImVec2 c(touch_tool_x[i] * ds.x, touch_tool_y * ds.y);
        float r = touch_tool_r * ds.y;
        bool down = touch_tool_down[i] != 0;
        if (touch_hide_state != 0) {
            if (i != 0) continue;
            dl->AddCircleFilled(c, r * 0.28f, rgba(255, 255, 255, 0.25f), 20);   // tiny dot: tap to bring everything back
            continue;
        }
        float a = 0.80f;
        dl->AddCircleFilled(c, r, rgba(15, 15, 25, down ? 0.80f : 0.55f), 36);
        dl->AddCircle(c, r, rgba(255, 255, 255, a), 36, 2.5f);
        ImU32 ink = rgba(255, 255, 255, 0.95f);
        if (i == 0) {   // eye
            ImVec2 pts[24];
            for (int k = 0; k < 24; k++) {
                float t = (float)k / 24.f * 6.2831853f;
                pts[k] = ImVec2(c.x + cosf(t) * r * 0.56f, c.y + sinf(t) * r * 0.30f);
            }
            dl->AddPolyline(pts, 24, ink, ImDrawFlags_Closed, r * 0.09f);
            dl->AddCircleFilled(c, r * 0.17f, ink, 16);
        } else {        // menu (3 lines)
            for (int k = -1; k <= 1; k++)
                dl->AddLine(ImVec2(c.x - r * 0.42f, c.y + k * r * 0.30f), ImVec2(c.x + r * 0.42f, c.y + k * r * 0.30f), ink, r * 0.13f);
        }
    }
}

static bool big_button(const char *label) {
    return ImGui::Button(label, ImVec2(-1.f, 44.f));
}

static void section_title(const char *text) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "%s", text);
    ImGui::Separator();
}

static void draw_quick_menu(ImVec2 ds) {
    static bool was_open = false;
    if (!touch_menu_open) { was_open = false; return; }
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(ds.x * 0.40f, ds.y), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.97f);
    if (!was_open) ImGui::SetNextWindowFocus();
    was_open = true;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.42f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.32f, 0.58f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.34f, 0.42f, 0.78f, 1.f));

    ImGuiWindowFlags fl = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##touch_quick_menu", NULL, fl)) {
        // ---- touch controls first: this is what is needed most on a phone ----
        section_title("TOUCH CONTROLS");
        if (big_button("Hide controls (clean shot)###q_hide")) {
            touch_hide_state = 1;       // everything hidden; a tiny dot (top right) brings it back
            touch_menu_open = 0;
            touch_layout_save();
        }
        if (big_button("Edit layout (move / resize)###q_edit")) {
            touch_edit = 1;
            touch_selected = -2;
            touch_menu_open = 0;
        }
        bool ena = touch_enabled != 0;
        if (ImGui::Checkbox("Controls enabled", &ena)) touch_enabled = ena ? 1 : 0;
        ImGui::SliderFloat("Opacity", &touch_opacity, 0.15f, 1.0f);
        ImGui::SliderFloat("Interface size", &touch_ui_mult, 0.6f, 2.0f);
        ImGui::TextWrapped("Eye icon (top right): hide / show the controls.");

        // ---- Saturn shortcuts ----
        section_title("SATURN");
        if (big_button(showMenu ? "Hide Saturn UI###q_ui" : "Show Saturn UI###q_ui")) showMenu = !showMenu;
        if (big_button("Settings###q_settings")) { windowSettings = !windowSettings; k_popout_open = false; }
        if (big_button("Timeline (keyframes)###q_timeline")) {
            k_popout_open = !k_popout_open;
            if (k_popout_open) windowSettings = false;
        }
        if (big_button("Chroma key###q_chroma")) {
            schroma_imgui_init();
            autoChroma = !autoChroma;
            windowCcEditor = false;
            windowAnimPlayer = false;
        }
        if (big_button(showStatusBars ? "Hide status bars###q_bars" : "Show status bars###q_bars")) showStatusBars = !showStatusBars;
        if (big_button(windowStats ? "Hide stats###q_stats" : "Show stats###q_stats")) windowStats = !windowStats;

        ImGui::Spacing();
        if (big_button("Close###q_close")) { touch_menu_open = 0; touch_layout_save(); }
    }
    ImGui::End();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
}

static void draw_edit_panel(ImVec2 ds) {
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.12f), ImGuiCond_Always, ImVec2(0.5f, 0.f));
    ImGui::SetNextWindowBgAlpha(0.95f);
    ImGuiWindowFlags fl = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    if (ImGui::Begin("##touch_edit_panel", NULL, fl)) {
        ImGui::TextUnformatted("Edit layout: drag a button to move it");
        ImGui::Separator();
        if (touch_selected >= 0) {
            ImGui::Text("Selected: %s", touch_buttons[touch_selected].name);
            ImGui::SliderFloat("Size", &touch_buttons[touch_selected].size, 0.03f, 0.20f);
            bool vis = touch_buttons[touch_selected].visible != 0;
            if (ImGui::Checkbox("Show this button", &vis)) touch_buttons[touch_selected].visible = vis ? 1 : 0;
        } else if (touch_selected == -1) {
            ImGui::TextUnformatted("Selected: stick");
            ImGui::SliderFloat("Size", &touch_stick_size, 0.08f, 0.35f);
        } else {
            ImGui::TextUnformatted("Tap a button or the stick.");
        }
        ImGui::SliderFloat("Opacity", &touch_opacity, 0.15f, 1.0f);
        if (ImGui::Button("Mirror")) touch_layout_mirror();
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { touch_layout_reset(); touch_selected = -2; }
        ImGui::SameLine();
        if (ImGui::Button("Done")) { touch_edit = 0; touch_selected = -2; touch_layout_save(); }
    }
    ImGui::End();
}

extern "C" void saturn_touch_ui(void) {
    ImGuiIO &io = ImGui::GetIO();
    touch_ui_capture = io.WantCaptureMouse ? 1 : 0;
    ImVec2 ds = io.DisplaySize;
    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // Toolbar taps
    int act = touch_take_action();
    if (act == TA_EYE) {
        touch_hide_state = touch_hide_state ? 0 : 1;   // eye: hide / show everything
        touch_menu_open = 0;
    } else if (act == TA_MENU && touch_hide_state == 0) {
        touch_menu_open = touch_menu_open ? 0 : 1;
        if (!touch_menu_open) touch_layout_save();
    }

    bool show_game = (touch_hide_state == 0) || touch_edit;
    if (show_game) {
        for (int i = 0; i < TB_COUNT; i++) {
            if (!touch_buttons[i].visible && !touch_edit) continue;
            float br = touch_buttons[i].size * ds.y;
            if (!touch_edit && touch_circle_overlaps_ui(touch_buttons[i].x, touch_buttons[i].y, br / ds.x, br / ds.y)) continue;
            ImVec2 c(touch_buttons[i].x * ds.x, touch_buttons[i].y * ds.y);
            draw_game_button(dl, c, touch_buttons[i].size * ds.y, i, touch_buttons[i].pressed != 0,
                             touch_edit && touch_selected == i, touch_buttons[i].visible == 0);
        }
        float sr = touch_stick_size * ds.y;
        if (touch_edit || touch_stick_active || !touch_circle_overlaps_ui(touch_stick_x, touch_stick_y, sr / ds.x, sr / ds.y))
            draw_stick(dl, ds, touch_edit && touch_selected == -1);
    }

    draw_toolbar(dl, ds);
    draw_quick_menu(ds);
    if (touch_edit) draw_edit_panel(ds);
}
