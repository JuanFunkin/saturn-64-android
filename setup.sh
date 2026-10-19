#!/usr/bin/env bash
# Version Linux de setup.ps1 (la usa GitHub Actions). Descarga SDL2 y Saturn y prepara el proyecto.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/_descargas"
JNI="$ROOT/app/src/main/jni"
ASSETS="$ROOT/app/src/main/assets"
JAVA_SDL="$ROOT/app/src/main/java/org/libsdl/app"

SDL_URL="https://github.com/libsdl-org/SDL/archive/refs/tags/release-2.28.5.zip"
SATURN_URL="https://github.com/Llennpie/Saturn/archive/5008553.zip"

mkdir -p "$TMP"
[ -f "$TMP/sdl.zip" ]    || curl -fL "$SDL_URL" -o "$TMP/sdl.zip"
[ -f "$TMP/saturn.zip" ] || curl -fL "$SATURN_URL" -o "$TMP/saturn.zip"

rm -rf "$TMP/sdl" "$TMP/saturn"
unzip -q "$TMP/sdl.zip" -d "$TMP/sdl"
unzip -q "$TMP/saturn.zip" -d "$TMP/saturn"
SDL="$(find "$TMP/sdl" -mindepth 1 -maxdepth 1 -type d | head -n1)"
SAT="$(find "$TMP/saturn" -mindepth 1 -maxdepth 1 -type d | head -n1)"

# 1) SDL2
rm -rf "$JNI/SDL2"; mkdir -p "$JNI/SDL2"
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

# 3) Cambios para Android (archivos modificados + generados)
cp -r "$ROOT"/overlay/saturn/. "$JNI/saturn/"

# 4) Archivos que Saturn lee al ejecutarse (dentro del APK)
rm -rf "$ASSETS"; mkdir -p "$ASSETS/textures"
cp -r "$SAT/dynos" "$ASSETS/dynos"
cp -r "$SAT/fonts" "$ASSETS/fonts"
[ -d "$SAT/textures/saturn" ] && cp -r "$SAT/textures/saturn" "$ASSETS/textures/saturn"
[ -f "$SAT/imgui.ini" ] && cp "$SAT/imgui.ini" "$ASSETS/imgui.ini"
echo "Listo."
