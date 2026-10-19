package com.saturn64.mobile;

import android.content.res.AssetManager;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Copia las carpetas que Saturn lee desde el directorio actual (dynos, fonts, textures)
 * desde los assets del APK a la carpeta interna de la app. No sobrescribe archivos
 * que ya existen (asi no se pierden proyectos ni ajustes del usuario).
 */
final class AssetInstaller {
    private static final String[] TOP = { "dynos", "fonts", "textures", "imgui.ini" };

    static void install(AssetManager am, File dest) throws IOException {
        for (String name : TOP) {
            copy(am, name, new File(dest, name));
        }
    }

    private static void copy(AssetManager am, String path, File dst) throws IOException {
        String[] list = am.list(path);
        if (list != null && list.length > 0) {
            dst.mkdirs();
            for (String n : list) {
                copy(am, path + "/" + n, new File(dst, n));
            }
            return;
        }
        if (dst.exists()) return;
        File parent = dst.getParentFile();
        if (parent != null) parent.mkdirs();
        try (InputStream in = am.open(path); OutputStream out = new FileOutputStream(dst)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        } catch (FileNotFoundException e) {
            dst.mkdirs(); // era una carpeta vacia
        }
    }

    private AssetInstaller() {}
}
