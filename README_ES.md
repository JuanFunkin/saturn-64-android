# Saturn 64 Mobile - Fase 2 (motor de Saturn en Android)

Esta version incluye el motor de Saturn (rama `legacy`, commit `5008553`) compilado con el NDK
para **32 bits (armeabi-v7a) y 64 bits (arm64-v8a)** en un solo APK, mas los controles tactiles.
**Es codigo sin probar**: lo esperable es que aparezcan errores de compilacion y de ejecucion y
los vamos corrigiendo uno por uno con el texto exacto del error.

## Antes de empezar (una sola vez)
1. Android Studio > **File > Settings > Languages & Frameworks > Android SDK > SDK Tools**.
2. Marca **NDK (Side by side)** y **CMake**, pulsa Apply y espera.

## Pasos
1. Descomprime este zip en una carpeta con ruta corta, por ejemplo `C:\saturn-android`.
2. Haz clic derecho en `setup.ps1` > **Ejecutar con PowerShell**. Descarga SDL2 y Saturn y prepara todo
   (necesita internet). Si Windows lo bloquea, abre PowerShell en esa carpeta y ejecuta:
   `powershell -ExecutionPolicy Bypass -File .\setup.ps1`
3. Android Studio > **File > Open** > elige la carpeta `saturn-android`. Espera el Gradle Sync.
4. **Build > Generate App Bundles or APKs > Generate APKs**. La primera compilacion tarda
   bastante (compila todo el juego dos veces: 32 y 64 bits).
5. Instala el APK (queda en `app/build/outputs/apk/debug/`) o usa el boton **Run** con el celular conectado.

## Que hace la app
- **Importar ROM**: abre el explorador del telefono, copia el ROM como `sm64.z64` y lo verifica.
- **Continuar**: copia los archivos de Saturn (dynos, fonts, textures) a la carpeta interna y arranca el motor.
  La primera vez, Saturn extrae los assets del ROM por su cuenta.
- **Controles tactiles**: stick flotante en la mitad izquierda, A, B, Z, Start, C (4), L y R.
  El boton **Controles** (abajo, al centro) abre el panel: mostrar/ocultar, activar/desactivar,
  opacidad, **editar posicion y tamano** (toca un boton y arrastralo), guardar y restablecer.
  Con "Mostrar controles" apagado la imagen queda limpia para fotos y videos.

## Interfaz de Saturn adaptada a celular
- **Escala automatica**: la interfaz se dibuja como en una ventana de ~600 px de alto y se amplia a la pantalla,
  asi los tamanos fijos de Saturn (listas, botones, ventanas) quedan proporcionados. En el panel **Controles**
  hay un deslizador **Tamano de la interfaz** para ajustarla.
- **Hojas en vez de ventanitas**: Ajustes, Editor de Color Code, Animation Mixtape y Chroma Key se abren como
  una hoja lateral a la derecha (42 % del ancho, alto completo). Solo hay una abierta a la vez.
  La **Linea de tiempo** (keyframes) es una hoja inferior a todo el ancho (45 % del alto); la hoja lateral
  se acorta para no taparla.
- **Estilo tactil**: margenes, barras de scroll y controles mas grandes.
- **Desplazar con el dedo**: arrastra sobre listas y ventanas para hacer scroll (no sobre un slider o boton).
- **Mantener presionado ~0.6 s = clic derecho** (menus contextuales, por ejemplo de keyframes).
- **Teclado en pantalla** al tocar un campo de texto.

## Que se cambio en el codigo de Saturn (carpeta `overlay/saturn`)
- `pc_main.c`, `saturn_rom_extract.cpp`: el ROM se busca en la carpeta interna de la app; los avisos usan SDL2.
- `gfx_sdl2.c`: sin MSAA, orientacion horizontal, eventos tactiles.
- `saturn_imgui.cpp`: GLSL de OpenGL ES (`#version 100`), interfaz mas grande, dibuja los controles tactiles.
- `controller_touch.c/.h`, `saturn_touch_ui.cpp`: controles tactiles (nuevos).
- `saturn_mobile_ui.cpp` (nuevo): escala, estilo, scroll con el dedo, clic derecho largo y teclado.
- `build/us_pc/...`: archivos que el Makefile de Saturn genera en el PC (no contienen nada del ROM).
- Discord Rich Presence desactivado.

## Cuando falle algo
- Error al compilar: abre la pestana **Build** de Android Studio y copia el primer error en rojo.
- Error al abrir la app (se cierra sola): conecta el celular, abre **Logcat**, filtra por `SDL` o `Saturn` y copia las ultimas lineas.

## Pendiente / riesgos conocidos
- La adaptacion de la interfaz es una primera version sin probar: puede haber ventanas con contenido cortado, botones
  muy pequenos o menus desplegables mas largos que la pantalla. Mandame capturas y lo ajusto ventana por ventana.
- Los menus de la barra superior (Menu, Camera, Appearance, Game) son desplegables de ImGui; en tactil funcionan con toques.
- El clic derecho largo tambien activa la accion normal del boton al soltar el dedo.
- Renderizado en OpenGL ES 2: pueden aparecer efectos o shaders que no funcionen.
- Dialogos de escritorio (`pfd`) en otros archivos de Saturn (modelos, DynOS): siguen usando codigo de PC.
- Rendimiento en 32 bits sin medir: probablemente haga falta bajar efectos.
- Captura de fotos/video: falta revisar que funcione en Android y que no salgan los controles.
