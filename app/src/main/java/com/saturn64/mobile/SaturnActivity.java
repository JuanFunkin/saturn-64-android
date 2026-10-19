package com.saturn64.mobile;

import org.libsdl.app.SDLActivity;

/** Arranca SDL2 y carga libSDL2.so + libmain.so (el motor de Saturn). */
public class SaturnActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
