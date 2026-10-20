#!/usr/bin/env bash
# Prepara el proyecto Android descargando SDL2 y Saturn y copiando sus archivos.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/_descargas"
JNI="$ROOT/app/src/main/jni"
ASSETS="$ROOT/app/src/main/assets"
JAVA_SDL="$ROOT/app/src/main/java/org/libsdl/app"
RES="$ROOT/app/src/main/res"

SDL_URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.28.5.zip"
SATURN_URL="https://github.com/Llennpie/Saturn/archive/5008553.zip"

mkdir -p "$TMP"
[ -f "$TMP/sdl.zip" ] || curl -fL "$SDL_URL" -o "$TMP/sdl.zip"
[ -f "$TMP/saturn.zip" ] || curl -fL "$SATURN_URL" -o "$TMP/saturn.zip"

rm -rf "$TMP/sdl" "$TMP/saturn"
mkdir -p "$TMP/sdl" "$TMP/saturn"
unzip -q "$TMP/sdl.zip" -d "$TMP/sdl"
unzip -q "$TMP/saturn.zip" -d "$TMP/saturn"

SDL="$(find "$TMP/sdl" -mindepth 1 -maxdepth 1 -type d | head -n1)"
SAT="$(find "$TMP/saturn" -mindepth 1 -maxdepth 1 -type d | head -n1)"

if [ -z "$SDL" ] || [ ! -d "$SDL/include" ] || [ ! -d "$SDL/src" ]; then
  echo "ERROR: No se encontró la carpeta SDL2 esperada dentro del ZIP." >&2
  exit 1
fi
if [ -z "$SAT" ] || [ ! -d "$SAT" ]; then
  echo "ERROR: No se encontró la carpeta de Saturn dentro del ZIP." >&2
  exit 1
fi

# 1) SDL2
rm -rf "$JNI/SDL2"
mkdir -p "$JNI/SDL2"
cp -r "$SDL/include" "$JNI/SDL2/include"
cp -r "$SDL/src" "$JNI/SDL2/src"
cp "$SDL/Android.mk" "$JNI/SDL2/Android.mk"

mkdir -p "$JAVA_SDL"
cp "$SDL"/android-project/app/src/main/java/org/libsdl/app/*.java "$JAVA_SDL/"

mkdir -p "$JNI/main/compat/SDL2"
cp -r "$SDL"/include/* "$JNI/main/compat/SDL2/"

# 2) Saturn
rm -rf "$JNI/saturn"
cp -r "$SAT" "$JNI/saturn"

# 3) Cambios específicos para Android
if [ ! -d "$ROOT/overlay/saturn" ]; then
  echo "ERROR: No existe overlay/saturn; no se pueden aplicar los cambios Android." >&2
  exit 1
fi
cp -r "$ROOT"/overlay/saturn/. "$JNI/saturn/"

# Android NDK define bcopy y bzero como macros en strings.h. Saturn declara
# esas funciones en PR/os_libc.h; desactivar las macros antes de esas
# declaraciones evita que el preprocesador corrompa las firmas.
OS_LIBC="$JNI/saturn/include/PR/os_libc.h"
if [ -f "$OS_LIBC" ]; then
  python3 - "$OS_LIBC" <<'PYFIX'
from pathlib import Path
import sys

p = Path(sys.argv[1])
s = p.read_text()
old = "extern void bcopy(const void *, void *, size_t);\nextern void bzero(void *, size_t);"
new = "#ifdef __ANDROID__\n#undef bcopy\n#undef bzero\n#endif\n\n" + old
if old in s and "#undef bcopy" not in s:
    p.write_text(s.replace(old, new, 1))
elif old not in s and ("extern void bcopy" in s or "extern void bzero" in s):
    raise SystemExit("No se reconoció el formato de las declaraciones bcopy/bzero en os_libc.h")
PYFIX
fi

# 4) Recursos que Saturn lee al ejecutarse (dentro del APK)
rm -rf "$ASSETS"
mkdir -p "$ASSETS/textures"
cp -r "$SAT/dynos" "$ASSETS/dynos"
cp -r "$SAT/fonts" "$ASSETS/fonts"
if [ -d "$SAT/textures/saturn" ]; then
  cp -r "$SAT/textures/saturn" "$ASSETS/textures/saturn"
fi
if [ -f "$SAT/imgui.ini" ]; then
  cp "$SAT/imgui.ini" "$ASSETS/imgui.ini"
fi

# 5) Asegurar que exista el icono que AndroidManifest.xml referencia como @mipmap/ic_launcher.
# Se crea un icono vectorial sencillo solo si el proyecto no contiene ya uno.
if ! find "$RES" -type f \( -name 'ic_launcher.png' -o -name 'ic_launcher.xml' -o -name 'ic_launcher.webp' \) -print -quit 2>/dev/null | grep -q .; then
  mkdir -p "$RES/mipmap"
  cat > "$RES/mipmap/ic_launcher.xml" <<'XML'
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="48dp"
    android:height="48dp"
    android:viewportWidth="48"
    android:viewportHeight="48">
    <path
        android:fillColor="#3949AB"
        android:pathData="M4,4h40v40h-40z" />
    <path
        android:fillColor="#FFFFFF"
        android:pathData="M14,12h20v5h-7.5v19h-5V17H14z" />
</vector>
XML
  echo "Se creó un icono Android provisional en res/mipmap/ic_launcher.xml."
fi

echo "Listo."
