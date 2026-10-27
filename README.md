# Saturn Studio para Android (32 y 64 bits)

Port de [Saturn Studio](https://github.com/fgetcat/saturn-studio) (fork de Saturn, editor de animaciones de SM64) a
teléfonos Android **armeabi-v7a (32 bits)** y **arm64-v8a (64 bits)**, con interfaz táctil.

## Cómo compilar (GitHub Actions)

1. Sube **todo el contenido de esta carpeta** a un repositorio tuyo (rama `main`).
2. Ve a *Actions → Build APK → Run workflow* (o simplemente haz push a `main`).
   - `abi = all` compila 32 + 64 bits. `abi = arm64` solo 64 bits (más rápido para pruebas).
3. Cuando termine, descarga el artefacto **saturn-studio-apk** y descomprímelo: ahí está el `.apk`.

El workflow usa las mismas versiones que tu port de Saturn 64: Java 17, Gradle 8.7, AGP 8.5.2, NDK 26.1.10909125,
compileSdk/targetSdk 34, minSdk 21, SDL2 2.28.5.

`setup.sh` (lo ejecuta el workflow) descarga SDL2 y Saturn Studio (commit fijo `afd5c38`), aplica `patches/*.patch`,
copia `overlay/` y genera los fuentes que el Makefile de PC genera (textos, headers de niveles, tabla de animaciones).

## Primer uso

1. Abre la app → **Dar acceso** (permiso "Acceso a todos los archivos"). Se crea `/storage/emulated/0/com.saturnstudio`.
   Si lo rechazas, usa el almacenamiento privado de la app.
2. **Importar ROM** → elige tu ROM de Super Mario 64 **US, .z64** (8 MB, CRC32 `3CE60709`). Se verifica y se copia a `com.saturnstudio/rom.z64`.
3. **Continuar**. La primera vez extrae los recursos (unos minutos); las siguientes abre directo el editor.

Todo se guarda en `com.saturnstudio/`: `dynos/` (proyectos, modelos, animaciones, temas), `res/` (recursos extraídos),
`exports/` (capturas y fotogramas exportados), `imports/` (archivos que eliges con el selector), configuración y registros
(`latest.log`, `stderr.log`, `crash.txt`).

## Controles táctiles

| Gesto | Acción |
|---|---|
| 1 dedo sobre la vista 3D | mover la cámara |
| 2 dedos sobre la vista 3D | rotar la cámara + pellizcar para zoom |
| Mantener pulsado (vista 3D) | clic derecho: seleccionar un Mario / abrir su menú |
| Toque corto (vista 3D) | clic; con la herramienta **Tap** activada, crea un Mario |
| 1 dedo sobre un panel | ratón (sliders, arrastrar, acoplar paneles) |
| 2 dedos sobre un panel | desplazar (scroll) |

Barra superior derecha: **Panels** (mostrar/ocultar paneles), **Tap**, **Pad** (gamepad N64 en pantalla, para mover a Mario),
**⚙** (velocidad de cámara, opacidad, tamaño de la UI, restablecer diseño) y **>** (contraer la barra).

## Qué se cambió respecto a PC

- OpenGL ES 3.0 (sin MSAA, sin modo alambre); captura por FBO + `glReadPixels`.
- Audio de 32 bits: parche de punteros en las estructuras de audio (igual que en tu port de Saturn 64) y `-fsigned-char`.
- Selector de archivos del sistema (SAF) en lugar de los diálogos de escritorio; guardados automáticos en `exports/`.
- Exportación de video: solo **secuencia PNG** (no hay FFmpeg en Android). Sin Discord, sin autoactualización, sin portapapeles de imágenes.
- UI de ImGui a escala táctil (fuentes nítidas, botones y barras de desplazamiento grandes).

## Si algo falla

Abre la app de nuevo: te ofrece **Compartir registro** (crash + logs). El workflow usa `-k`, así que un fallo de compilación
lista **todos** los errores de una sola vez.
