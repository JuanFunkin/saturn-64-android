#ifndef CONTROLLER_TOUCH_H
#define CONTROLLER_TOUCH_H

// Controles tactiles para Saturn 64 Mobile (Android).
// Las posiciones son normalizadas: x,y en [0,1] (centro del boton respecto a la ventana)
// y size = radio como fraccion de la ALTURA de la pantalla.

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
    const char *name;     // nombre interno (para guardar el layout)
    const char *label;    // texto dibujado en el boton
    float x, y, size;
    unsigned int mask;    // mascara N64 (A_BUTTON, B_BUTTON, ...)
    int pressed;
    SDL_FingerID finger;
} TouchButton;

extern TouchButton touch_buttons[TB_COUNT];

// Stick virtual (flotante: se ancla donde pones el dedo, dentro de la zona izquierda)
extern float touch_stick_x, touch_stick_y, touch_stick_size;
extern float touch_stick_ox, touch_stick_oy;   // origen cuando esta activo
extern float touch_stick_dx, touch_stick_dy;   // desplazamiento normalizado [-1,1]
extern int   touch_stick_active;

extern int   touch_visible;      // dibujar los controles
extern int   touch_enabled;      // los controles responden al tacto (aunque esten ocultos)
extern int   touch_edit;         // modo edicion (arrastrar botones)
extern int   touch_selected;     // boton seleccionado en edicion (-1 = stick, -2 = ninguno)
extern float touch_opacity;      // 0.1 - 1.0
extern float touch_ui_mult;      // multiplicador del tamano de la interfaz de Saturn (0.7 - 1.8)
extern int   touch_ui_capture;   // lo pone la interfaz: 1 si ImGui esta usando el toque

void touch_handle_event(const SDL_Event *e);
void touch_layout_save(void);
void touch_layout_load(void);
void touch_layout_reset(void);

struct ControllerAPI;
extern struct ControllerAPI controller_touch;

#ifdef __cplusplus
}
#endif

#endif
