// Dibujo de los controles tactiles y panel de configuracion (ImGui).
// Se llama una vez por frame desde saturn_imgui_update(), justo antes de ImGui::Render().
// Todo se dibuja con la lista "foreground" de ImGui, asi que al ocultar los controles
// (o la interfaz de Saturn) la imagen del juego queda limpia.

#include <string.h>
#include "saturn/libs/imgui/imgui.h"
#include "pc/controller/controller_touch.h"

static bool panel_open = false;

static ImU32 col(float r, float g, float b, float a) {
    return ImGui::GetColorU32(ImVec4(r, g, b, a));
}

static void draw_circle_button(ImDrawList *dl, ImVec2 c, float r, const char *label, bool pressed, bool selected) {
    float a = touch_opacity;
    dl->AddCircleFilled(c, r, pressed ? col(1.f, 1.f, 1.f, a * 0.9f) : col(0.1f, 0.1f, 0.1f, a * 0.6f), 32);
    dl->AddCircle(c, r, selected ? col(1.f, 0.8f, 0.1f, 1.f) : col(1.f, 1.f, 1.f, a), 32, selected ? 4.f : 2.f);
    ImVec2 ts = ImGui::CalcTextSize(label);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f),
                pressed ? col(0.f, 0.f, 0.f, 1.f) : col(1.f, 1.f, 1.f, a + 0.2f), label);
}

extern "C" void saturn_touch_ui(void) {
    ImGuiIO &io = ImGui::GetIO();
    touch_ui_capture = io.WantCaptureMouse ? 1 : 0;

    ImVec2 ds = io.DisplaySize;
    ImDrawList *dl = ImGui::GetForegroundDrawList();

    if (touch_visible || touch_edit) {
        for (int i = 0; i < TB_COUNT; i++) {
            ImVec2 c(touch_buttons[i].x * ds.x, touch_buttons[i].y * ds.y);
            draw_circle_button(dl, c, touch_buttons[i].size * ds.y, touch_buttons[i].label,
                               touch_buttons[i].pressed != 0, touch_edit && touch_selected == i);
        }
        // Stick
        float sr = touch_stick_size * ds.y;
        ImVec2 base = touch_stick_active
            ? ImVec2(touch_stick_ox * ds.x, touch_stick_oy * ds.y)
            : ImVec2(touch_stick_x * ds.x, touch_stick_y * ds.y);
        bool sel = touch_edit && touch_selected == -1;
        dl->AddCircle(base, sr, sel ? col(1.f, 0.8f, 0.1f, 1.f) : col(1.f, 1.f, 1.f, touch_opacity), 40, sel ? 4.f : 2.f);
        ImVec2 knob(base.x + touch_stick_dx * sr, base.y + touch_stick_dy * sr);
        dl->AddCircleFilled(knob, sr * 0.4f, col(1.f, 1.f, 1.f, touch_opacity * 0.8f), 32);
    }

    // Boton pequeno para abrir el panel (siempre disponible, bajo en opacidad)
    ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y - 8.f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.f);
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("##touch_gear", NULL, fl)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, panel_open ? 1.f : 0.45f);
        if (ImGui::Button(panel_open ? "Close" : "Controls")) panel_open = !panel_open;
        ImGui::PopStyleVar();
    }
    ImGui::End();

    if (panel_open) {
        ImGui::SetNextWindowPos(ImVec2(ds.x * 0.5f, ds.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Touch controls", &panel_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            bool vis = touch_visible != 0;
            bool ena = touch_enabled != 0;
            bool edi = touch_edit != 0;
            if (ImGui::Checkbox("Show controls", &vis)) touch_visible = vis;
            if (ImGui::Checkbox("Controls enabled", &ena)) touch_enabled = ena;
            ImGui::SliderFloat("Opacity", &touch_opacity, 0.1f, 1.0f);
            ImGui::SliderFloat("Interface size", &touch_ui_mult, 0.7f, 1.8f);
            if (ImGui::Checkbox("Edit position and size", &edi)) {
                touch_edit = edi;
                if (edi) touch_visible = 1;
                touch_selected = -2;
            }
            if (touch_edit) {
                ImGui::TextUnformatted("Tap a button and drag it.");
                if (touch_selected >= 0) {
                    ImGui::Text("Selected: %s", touch_buttons[touch_selected].name);
                    ImGui::SliderFloat("Size", &touch_buttons[touch_selected].size, 0.03f, 0.20f);
                } else if (touch_selected == -1) {
                    ImGui::TextUnformatted("Selected: stick");
                    ImGui::SliderFloat("Size", &touch_stick_size, 0.08f, 0.35f);
                }
                if (ImGui::Button("Save")) touch_layout_save();
                ImGui::SameLine();
                if (ImGui::Button("Reset")) touch_layout_reset();
            }
            ImGui::TextUnformatted("Hide controls for clean photos and videos.");
        }
        ImGui::End();
    }
}
