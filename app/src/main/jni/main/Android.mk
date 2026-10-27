# Saturn Studio for Android: builds the engine as libmain.so (SDL2 + OpenGL ES 3.0).
# The source list mirrors the PC Makefile (SRC_DIRS / levels / goddard / lib/src / sound), minus the
# Windows, SDL1, legacy GL, emscripten and Discord files.
LOCAL_PATH := $(call my-dir)
ST := ../studio
STABS := $(LOCAL_PATH)/$(ST)

include $(CLEAR_VARS)
LOCAL_MODULE := main

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/compat \
    $(LOCAL_PATH)/compat/SDL2 \
    $(STABS)/include \
    $(STABS)/build/us_pc \
    $(STABS)/build/us_pc/include \
    $(STABS)/src \
    $(STABS)

LOCAL_CFLAGS := \
    -DDYNOS -DRAPI_GL=1 -DWAPI_SDL2=1 -DAAPI_SDL2=1 -DCAPI_SDL2=1 -DHAVE_SDL2=1 \
    -DVERSION_US -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DF3DEX_GBI_2E \
    -O0 -fno-strict-aliasing -fwrapv -fcommon -fsigned-char -funwind-tables \
    -fno-delete-null-pointer-checks -fno-inline-functions \
    -DGIT_HASH=\"afd5c38\" -DGIT_BRANCH=\"android\" \
    -DNODRAWINGDISTANCE -DTEXTURE_FIX -DEXT_OPTIONS_MENU -DEXTERNAL_DATA \
    -DFS_BASEDIR=\"res\" \
    -DUSE_GLES -DIMGUI_IMPL_OPENGL_ES3 -D_GNU_SOURCE \
    -Wno-int-conversion -Wno-implicit-function-declaration -Wno-incompatible-pointer-types \
    -Wno-implicit-int -Wno-deprecated-declarations -Wno-narrowing -Wno-everything

# The desktop Makefile builds the SM64 decomp C code with DEBUG=1 (no optimisation); the decomp relies on undefined
# behaviour that Clang -O2 breaks (crash right after the editor starts). C stays at -O0, C++ (ImGui etc.) is optimised.
LOCAL_CPPFLAGS := -O1 -std=c++17 -fexceptions -frtti
LOCAL_DISABLE_FORMAT_STRING_CHECKS := true

SRC_DIRS := src src/engine src/game src/audio src/menu src/buffers actors levels bin data assets \
            src/pc src/pc/gfx src/pc/audio src/pc/controller src/pc/fs src/pc/fs/packtypes \
            src/saturn src/saturn/imgui src/saturn/libs/imgui src/saturn/filesystem src/saturn/cmd \
            src/saturn/touch src/saturn/discord
GODDARD_DIRS := src/goddard src/goddard/dynlists
ULTRA_C := alBnkfNew guLookAtRef guMtxF2L guNormalize guOrthoF guPerspectiveF guRotateF guScaleF guTranslateF ldiv

ALL_C := $(foreach d,$(SRC_DIRS),$(wildcard $(STABS)/$(d)/*.c)) \
         $(wildcard $(STABS)/levels/*/leveldata.c) $(wildcard $(STABS)/levels/*/script.c) $(wildcard $(STABS)/levels/*/geo.c) \
         $(foreach d,$(GODDARD_DIRS),$(wildcard $(STABS)/$(d)/*.c)) \
         $(foreach f,$(ULTRA_C),$(STABS)/lib/src/$(f).c) \
         $(STABS)/sound/sound_data.c \
         $(STABS)/build/us_pc/assets/mario_anim_data.c
ALL_CXX := $(foreach d,$(SRC_DIRS),$(wildcard $(STABS)/$(d)/*.cpp))

# Not for Android: desktop OpenGL 2 ImGui backend, main() of the N64 build, Windows/DirectX, SDL1, legacy GL, emscripten, Discord (stubbed instead)
EXCLUDE := \
    $(STABS)/src/game/main.c \
    $(STABS)/src/pc/gfx/gfx_direct3d11.cpp $(STABS)/src/pc/gfx/gfx_direct3d12.cpp \
    $(STABS)/src/pc/gfx/gfx_direct3d_common.cpp $(STABS)/src/pc/gfx/gfx_dxgi.cpp \
    $(STABS)/src/pc/gfx/gfx_sdl1.c $(STABS)/src/pc/gfx/gfx_opengl_legacy.c \
    $(STABS)/src/pc/audio/audio_sdl1.c $(STABS)/src/pc/controller/controller_sdl1.c \
    $(STABS)/src/pc/controller/controller_emscripten_keyboard.c \
    $(STABS)/src/saturn/discord/saturn_discord.cpp \
    $(STABS)/src/saturn/libs/imgui/imgui_impl_opengl2.cpp

SRCS := $(sort $(filter-out $(EXCLUDE),$(ALL_C) $(ALL_CXX)))
LOCAL_SRC_FILES := $(patsubst $(LOCAL_PATH)/%,%,$(SRCS))

LOCAL_LDFLAGS := -Wl,--error-limit=0 -Wl,--export-dynamic
LOCAL_LDLIBS := -lGLESv3 -llog -landroid -lm -ldl

include $(BUILD_SHARED_LIBRARY)
