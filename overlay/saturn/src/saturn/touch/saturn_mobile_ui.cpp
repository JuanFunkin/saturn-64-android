// Adaptacion de la interfaz de Saturn (ImGui) para pantallas tactiles.
//
//  1. Escala: ImGui trabaja en "pixeles logicos" (pantalla / escala) y el backend lo amplia al tamano real.
//     Asi todos los tamanos fijos de Saturn (ventanas, listas, botones) se ven proporcionados en un celular.
//  2. Estilo tactil: margenes, barras de scroll y controles mas grandes.
//  3. Arrastrar con el dedo para desplazar listas y ventanas (ImGui solo trae scroll con rueda del mouse).
//  4. Mantener presionado ~0.6 s = clic derecho (menus contextuales de Saturn).
//  5. Teclado en pantalla cuando un campo de texto lo pide.

#include <math.h>
#include <float.h>
#include <SDL2/SDL.h>
#include "saturn/libs/imgui/imgui.h"
#include "saturn/libs/imgui/imgui_internal.h"

extern "C" {
#include "pc/controller/controller_touch.h"
}

float mobile_scale_base = 1.0f;   // escala con la que se dibuja la tipografia (fija tras iniciar)

// Altura logica objetivo: la interfaz de PC de Saturn esta pensada para ventanas de ~600 px de alto.
void saturn_mobile_init_scale(SDL_Window *w) {
    int ww = 1280, hh = 720;
    if (w) SDL_GetWindowSize(w, &ww, &hh);
    // The UI is laid out as if the screen were ~340 logical pixels tall (about 3x bigger than on PC),
    // so text and buttons are comfortable for fingers on any phone.
    float s = (float)hh / 340.0f;
    if (s < 1.6f) s = 1.6f;
    if (s > 5.0f) s = 5.0f;
    mobile_scale_base = s;
}

// Se llama entre ImGui_ImplSDL2_NewFrame() e ImGui::NewFrame().
void saturn_mobile_begin_frame(void) {
    ImGuiIO &io = ImGui::GetIO();
    float S = mobile_scale_base * touch_ui_mult;
    if (S < 0.5f) S = 0.5f;
    io.DisplaySize.x /= S;
    io.DisplaySize.y /= S;
    io.DisplayFramebufferScale.x *= S;
    io.DisplayFramebufferScale.y *= S;
    if (io.MousePos.x > -FLT_MAX / 2.f) {
        io.MousePos.x /= S;
        io.MousePos.y /= S;
    }
    io.FontGlobalScale = 1.0f / mobile_scale_base;
    io.MouseDragThreshold = 10.0f;
}

// Se llama al final de imgui_update_theme(): margenes y controles grandes para dedos.
void saturn_mobile_style(void) {
    ImGuiStyle &st = ImGui::GetStyle();
    st.WindowPadding = ImVec2(10.f, 10.f);
    st.FramePadding = ImVec2(10.f, 9.f);
    st.ItemSpacing = ImVec2(10.f, 11.f);
    st.ItemInnerSpacing = ImVec2(8.f, 8.f);
    st.TouchExtraPadding = ImVec2(6.f, 6.f);
    st.ScrollbarSize = 22.f;
    st.GrabMinSize = 24.f;
    st.WindowMinSize = ImVec2(90.f, 60.f);
    st.PopupRounding = 8.f;
    st.FrameRounding = 6.f;
}

// Se llama al final del frame, antes de ImGui::Render().
void saturn_mobile_end_frame(void) {
    ImGuiContext *g = ImGui::GetCurrentContext();
    ImGuiIO &io = ImGui::GetIO();

    // --- Teclado en pantalla ---
    static bool kb_on = false;
    if (io.WantTextInput && !kb_on) { SDL_StartTextInput(); kb_on = true; }
    else if (!io.WantTextInput && kb_on) { SDL_StopTextInput(); kb_on = false; }

    // --- Desplazar arrastrando ---
    static ImGuiWindow *scroll_win = NULL;
    if (ImGui::IsMouseDown(0)) {
        if (scroll_win == NULL && ImGui::IsMouseDragging(0) && ImGui::GetActiveID() == 0 && g->HoveredWindow) {
            ImGuiWindow *w = g->HoveredWindow;
            while (w && w->ScrollMax.y <= 0.f && w->ScrollMax.x <= 0.f && w->ParentWindow) w = w->ParentWindow;
            if (w && (w->ScrollMax.y > 0.f || w->ScrollMax.x > 0.f)) scroll_win = w;
        }
        if (scroll_win) {
            if (scroll_win->ScrollMax.y > 0.f) ImGui::SetScrollY(scroll_win, scroll_win->Scroll.y - io.MouseDelta.y);
            if (scroll_win->ScrollMax.x > 0.f) ImGui::SetScrollX(scroll_win, scroll_win->Scroll.x - io.MouseDelta.x);
        }
    } else {
        scroll_win = NULL;
    }

    // --- Mantener presionado = clic derecho ---
    static float held = 0.f;
    static ImVec2 start;
    static int lp_state = 0;   // 0 = esperando, 1 = clic derecho enviado (soltar en el siguiente frame), 2 = ya disparado
    if (lp_state == 1) {
        io.AddMouseButtonEvent(1, false);
        lp_state = 2;
    }
    if (ImGui::IsMouseDown(0)) {
        if (ImGui::IsMouseClicked(0)) { held = 0.f; start = io.MousePos; lp_state = 0; }
        held += io.DeltaTime;
        float dx = io.MousePos.x - start.x, dy = io.MousePos.y - start.y;
        if (lp_state == 0 && scroll_win == NULL && held > 0.6f && (dx * dx + dy * dy) < 100.f) {
            io.AddMouseButtonEvent(1, true);
            lp_state = 1;
        }
    } else {
        held = 0.f;
        if (lp_state == 2) lp_state = 0;
    }
}

// Called after saturn_touch_ui(), right before ImGui::Render(): remembers where the visible windows are so that
// touches on them do not also move Mario (see touch_handle_event).
void saturn_mobile_collect_ui_rects(void) {
    ImGuiContext *g = ImGui::GetCurrentContext();
    ImGuiIO &io = ImGui::GetIO();
    touch_ui_rects_begin();
    if (io.DisplaySize.x <= 0.f || io.DisplaySize.y <= 0.f) return;
    for (int i = 0; i < g->Windows.Size; i++) {
        ImGuiWindow *w = g->Windows[i];
        if (!w->Active || w->Hidden) continue;
        if (w->Flags & (ImGuiWindowFlags_ChildWindow | ImGuiWindowFlags_NoMouseInputs | ImGuiWindowFlags_Tooltip)) continue;
        ImRect r = w->Rect();
        if (r.GetWidth() > io.DisplaySize.x * 0.97f && r.GetHeight() > io.DisplaySize.y * 0.97f) continue; // full-screen backdrops
        touch_ui_rect_add(r.Min.x / io.DisplaySize.x, r.Min.y / io.DisplaySize.y,
                          r.Max.x / io.DisplaySize.x, r.Max.y / io.DisplaySize.y);
    }
}
