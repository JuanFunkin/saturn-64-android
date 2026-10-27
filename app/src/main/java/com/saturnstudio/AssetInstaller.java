package com.saturnstudio;

import android.content.res.AssetManager;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Copies the folders Saturn Studio reads from its working directory (dynos, fonts, textures) from the APK
 * assets into the data folder (com.saturnstudio). Existing files are never overwritten, so the user's own
 * models, packs, animations, projects and settings are kept. Also creates the (possibly empty) folders the
 * program expects: git does not store empty folders and the APK packer drops ".gitkeep".
 */
final class AssetInstaller {
    private static final String[] TOP = { "dynos", "fonts", "textures" };

    private static final String[] REQUIRED_DIRS = {
        "res", "exports", "imports",
        "dynos", "dynos/macros", "dynos/packs", "dynos/projects", "dynos/anims",
        "dynos/themes", "dynos/eyes", "dynos/colorcodes", "dynos/textures",
        "textures"
    };

    static void install(AssetManager am, File dest) throws IOException {
        for (String name : TOP) copy(am, name, new File(dest, name));
        for (String d : REQUIRED_DIRS) new File(dest, d).mkdirs();
    }

    private static void copy(AssetManager am, String path, File dst) throws IOException {
        String[] list = am.list(path);
        if (list != null && list.length > 0) {
            dst.mkdirs();
            for (String n : list) copy(am, path + "/" + n, new File(dst, n));
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
            dst.mkdirs(); // it was an empty folder
        }
    }

    private AssetInstaller() {}
}
