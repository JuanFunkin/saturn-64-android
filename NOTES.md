# Notas de desarrollo — Saturn Studio Android

- Estado: código completo (Java, NDK, parches, overlay, setup.sh, workflow). NO compilado a APK en el entorno de desarrollo
  (sin acceso al NDK); verificado con clang en el PC (-fsyntax-only de las 350 fuentes) y aplicando parches sobre el commit fijado.
- Commit de Studio fijado: afd5c38413cffb7b6de134d7acb2ebf7c4868fec. SDL2: release-2.28.5.
- Regenerar parches: editar una copia de Studio y hacer `diff -u` por archivo hacia patches/NN-*.patch.
- Archivos nuevos: overlay/src/pc/{android_support.*,controller/controller_touch.*}, overlay/src/saturn/touch/*,
  overlay/src/saturn/discord/android_discord_stub.cpp, overlay/include/audio_ptr.h, overlay/compat/GL/glew.h.
- Puntos a vigilar en el primer build real: libc++ (includes de <algorithm>/<iostream>), símbolos GLES3 en imgui_impl_opengl3,
  enlace de libmain.so (-lGLESv3), arranque SDL (SDL_main), JNI nativeOnFilePicked.
