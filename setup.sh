#!/usr/bin/env bash
# Prepares the Android project: downloads SDL2 and Saturn Studio (pinned commit), applies the Android patches
# and overlay, generates the sources the PC Makefile would generate, and copies the runtime assets.
#
# Optional (for offline tests): SDL_SRC_DIR=/path/to/SDL  STUDIO_SRC_DIR=/path/to/saturn-studio
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
TMP="$ROOT/_descargas"
JNI="$ROOT/app/src/main/jni"
ASSETS="$ROOT/app/src/main/assets"
JAVA_SDL="$ROOT/app/src/main/java/org/libsdl/app"
RES="$ROOT/app/src/main/res"

SDL_REPO="https://github.com/libsdl-org/SDL.git"
SDL_TAG="release-2.28.5"
STUDIO_REPO="https://github.com/fgetcat/saturn-studio.git"
STUDIO_SHA="afd5c38413cffb7b6de134d7acb2ebf7c4868fec"

mkdir -p "$TMP"

fetch_sdl() {
  if [ -n "${SDL_SRC_DIR:-}" ]; then SDL="$SDL_SRC_DIR"; return; fi
  SDL="$TMP/SDL"
  if [ ! -d "$SDL/include" ]; then
    rm -rf "$SDL"
    git clone --depth 1 --branch "$SDL_TAG" "$SDL_REPO" "$SDL"
  fi
}

fetch_studio() {
  if [ -n "${STUDIO_SRC_DIR:-}" ]; then SAT="$STUDIO_SRC_DIR"; return; fi
  SAT="$TMP/studio"
  if [ ! -f "$SAT/Makefile" ]; then
    rm -rf "$SAT"
    mkdir -p "$SAT"
    git -C "$SAT" init -q
    git -C "$SAT" remote add origin "$STUDIO_REPO"
    git -C "$SAT" fetch -q --depth 1 origin "$STUDIO_SHA"
    git -C "$SAT" checkout -q FETCH_HEAD
  fi
}

fetch_sdl
fetch_studio

if [ ! -d "$SDL/include" ] || [ ! -d "$SDL/src" ] || [ ! -f "$SDL/Android.mk" ]; then
  echo "ERROR: SDL2 folder not found/incomplete." >&2; exit 1
fi
if [ ! -f "$SAT/Makefile" ] || [ ! -d "$SAT/src/saturn" ]; then
  echo "ERROR: Saturn Studio folder not found/incomplete." >&2; exit 1
fi

# 1) SDL2 (+ its Java classes)
rm -rf "$JNI/SDL2"
mkdir -p "$JNI/SDL2"
cp -r "$SDL/include" "$JNI/SDL2/include"
cp -r "$SDL/src" "$JNI/SDL2/src"
cp "$SDL/Android.mk" "$JNI/SDL2/Android.mk"

rm -rf "$JAVA_SDL"
mkdir -p "$JAVA_SDL"
cp "$SDL"/android-project/app/src/main/java/org/libsdl/app/*.java "$JAVA_SDL/"

# Studio includes <SDL2/SDL.h>
rm -rf "$JNI/main/compat"
mkdir -p "$JNI/main/compat/SDL2"
cp -r "$SDL"/include/* "$JNI/main/compat/SDL2/"

# 2) Saturn Studio sources (clean copy, without .git)
rm -rf "$JNI/studio"
mkdir -p "$JNI/studio"
( cd "$SAT" && tar --exclude=.git -cf - . ) | ( cd "$JNI/studio" && tar -xf - )

# 3) Android patches (32/64-bit audio fix + OpenGL ES 3 + touch + storage) and new files
for p in "$ROOT"/patches/*.patch; do
  echo "patch: $(basename "$p")"
  patch -p1 --no-backup-if-mismatch -s -d "$JNI/studio" < "$p"
done
cp -r "$ROOT/overlay/src" "$JNI/studio/"
cp -r "$ROOT/overlay/include" "$JNI/studio/"
cp -r "$ROOT/overlay/compat/." "$JNI/main/compat/"

# 4) Files the PC Makefile generates before compiling (they only depend on text, not on the ROM):
#    text strings, level headers, mario animation table, level rules and the dynos copy.
echo "Generating sources with the Studio Makefile..."
(
  cd "$JNI/studio"
  PY=python3
  command -v python3 >/dev/null 2>&1 || PY=python
  # The Makefile also looks for SDL2/libzip through pkg-config; those messages are harmless here.
  make -k VERSION=us PYTHON=$PY DISCORDRPC=0 DISCORDGAMESDK=0 \
    build/us_pc/include/text_strings.h \
    build/us_pc/include/level_headers.h \
    build/us_pc/text/us/define_text.inc.c 2>&1 | grep -v "^Package\|^Perhaps\|^to the PKG\|virtual:world\|not a git" || true
)
for f in build/us_pc/include/text_strings.h build/us_pc/include/level_headers.h \
         build/us_pc/text/us/define_text.inc.c build/us_pc/assets/mario_anim_data.c; do
  if [ ! -s "$JNI/studio/$f" ]; then
    echo "ERROR: the Studio Makefile did not generate $f" >&2
    exit 1
  fi
done
# host build products are useless (and big) for the Android build
rm -rf "$JNI/studio/tools/"*.o 2>/dev/null || true

# 5) Runtime files Studio reads from its working directory (installed into com.saturnstudio on first start)
rm -rf "$ASSETS"
mkdir -p "$ASSETS"
cp -r "$SAT/dynos" "$ASSETS/dynos"
cp -r "$SAT/fonts" "$ASSETS/fonts"
[ -d "$SAT/textures/saturn" ] && { mkdir -p "$ASSETS/textures"; cp -r "$SAT/textures/saturn" "$ASSETS/textures/saturn"; }
# some folders are empty in git: put a placeholder so the APK keeps them (the app also creates them)
find "$ASSETS" -type f -name '.gitkeep' -delete 2>/dev/null || true

# 6) Launcher icon (simple vector; replace app/src/main/res/mipmap-*/ic_launcher.png to customize)
if ! find "$RES" -type f \( -name 'ic_launcher.png' -o -name 'ic_launcher.xml' -o -name 'ic_launcher.webp' \) -print -quit 2>/dev/null | grep -q .; then
  mkdir -p "$RES/mipmap-anydpi"
  cat > "$RES/mipmap-anydpi/ic_launcher.xml" <<'XML'
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="108dp"
    android:height="108dp"
    android:viewportWidth="108"
    android:viewportHeight="108">
    <path android:fillColor="#1B1B4B" android:pathData="M0,0h108v108h-108z" />
    <path android:fillColor="#F2C14E" android:pathData="M54,54m-20,0a20,20 0,1 1,40 0a20,20 0,1 1,-40 0" />
    <path android:fillColor="#00000000" android:strokeColor="#FFFFFF" android:strokeWidth="4"
        android:pathData="M22,62C34,40 74,40 86,50C74,70 34,72 22,62z" />
</vector>
XML
fi

echo "Listo / Done."
