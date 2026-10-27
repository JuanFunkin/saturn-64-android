package com.saturnstudio;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Runs SDL2 + the Saturn Studio engine (libSDL2.so + libmain.so).
 *
 *  - Shows a loading overlay with the progress the engine writes to extract_status.txt (the first start extracts
 *    the ROM assets and can take a few minutes).
 *  - running.flag: created at start, removed on a normal exit, so the launcher can tell when the last run
 *    ended badly and offer to share the log.
 *  - pickFile(): called from the engine (JNI) when Studio needs a file (models, images...). It opens the system
 *    file picker, copies the chosen file to <data>/imports and hands the path back to the engine.
 */
public class SaturnActivity extends SDLActivity {

    private static final int REQ_PICK = 4001;

    private static SaturnActivity instance;

    private TextView overlay;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private File dataDir;
    private String lastStatus = "";
    private long lastStatusChange = 0;

    /** Engine -> Java result of pickFile(): absolute path of the copied file, or null if cancelled. */
    public static native void nativeOnFilePicked(String path);

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    private File readDataDir() {
        File cfg = new File(getFilesDir(), "datadir.txt");
        try (BufferedReader r = new BufferedReader(new FileReader(cfg))) {
            String line = r.readLine();
            if (line != null && !line.trim().isEmpty()) return new File(line.trim());
        } catch (IOException e) {
            // fall through
        }
        return getFilesDir();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        instance = this;
        dataDir = readDataDir();
        try {
            new File(getFilesDir(), "running.flag").createNewFile();
        } catch (IOException e) {
            // not critical
        }
        super.onCreate(savedInstanceState);
        addOverlay();
        lastStatusChange = System.currentTimeMillis();
        handler.post(pollStatus);
    }

    private void addOverlay() {
        if (mLayout == null) return;
        overlay = new TextView(this);
        overlay.setBackgroundColor(0xE6000000);
        overlay.setTextColor(Color.WHITE);
        overlay.setTextSize(19);
        overlay.setGravity(Gravity.CENTER);
        overlay.setPadding(48, 48, 48, 48);
        overlay.setText("Starting Saturn Studio...");
        mLayout.addView(overlay, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
    }

    private final Runnable pollStatus = new Runnable() {
        @Override
        public void run() {
            if (overlay != null) {
                String text = null;
                boolean hide = false;
                File f = new File(dataDir, "extract_status.txt");
                if (f.exists()) {
                    try (BufferedReader r = new BufferedReader(new FileReader(f))) {
                        String first = r.readLine();
                        String msg = r.readLine();
                        float p = Float.parseFloat(first == null ? "0" : first.trim());
                        if ("extracting".equals(msg)) {
                            text = "Extracting assets from the ROM...\n" + Math.round(p * 100) + "%\n\n(first start only)";
                        } else if ("extracted".equals(msg) || p >= 1f) {
                            text = "Loading...";
                        } else if (p < 0) {
                            text = "Error: " + ("rom_missing".equals(msg) ? "the ROM is missing" : "the ROM is not valid")
                                    + ".\nClose the app and import it again.";
                        } else {
                            text = "Starting Saturn Studio...";
                        }
                    } catch (IOException | NumberFormatException e) {
                        // the engine is rewriting the file: try again next time
                    }
                } else {
                    text = "Starting Saturn Studio...";
                }
                // the engine hides the overlay by writing "ready" once the editor is on screen
                File ready = new File(dataDir, "ready.flag");
                if (ready.exists()) hide = true;

                long now = System.currentTimeMillis();
                if (text != null && !text.equals(lastStatus)) {
                    lastStatus = text;
                    lastStatusChange = now;
                }
                if (hide) {
                    overlay.setVisibility(android.view.View.GONE);
                } else if (text != null) {
                    overlay.setVisibility(android.view.View.VISIBLE);
                    if (now - lastStatusChange > 60000) {
                        text += "\n\nThis is taking a long time. If it stays like this, close the app and open it again:"
                                + " it will offer to share the log.";
                    }
                    overlay.setText(text);
                }
            }
            handler.postDelayed(this, 400);
        }
    };

    @Override
    protected void onDestroy() {
        handler.removeCallbacks(pollStatus);
        super.onDestroy();
        if (isFinishing()) {
            new File(getFilesDir(), "running.flag").delete();   // normal exit
        }
        if (instance == this) instance = null;
    }

    // ------------------------------------------------------------------ called from native code (JNI)

    public static void toast(final String msg) {
        final SaturnActivity a = instance;
        if (a == null) return;
        a.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Toast.makeText(a, msg, Toast.LENGTH_LONG).show();
            }
        });
    }

    public static void pickFile(final String title, final String mime) {
        final SaturnActivity a = instance;
        if (a == null) {
            nativeOnFilePicked(null);
            return;
        }
        a.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    i.addCategory(Intent.CATEGORY_OPENABLE);
                    i.setType(mime == null || mime.isEmpty() ? "*/*" : mime);
                    a.startActivityForResult(i, REQ_PICK);
                } catch (Exception e) {
                    nativeOnFilePicked(null);
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQ_PICK) return;
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            nativeOnFilePicked(null);
            return;
        }
        final Uri uri = data.getData();
        new Thread(new Runnable() {
            @Override
            public void run() {
                String result = null;
                try {
                    result = copyToImports(uri);
                } catch (Exception e) {
                    toast("Could not open the file: " + e.getMessage());
                }
                nativeOnFilePicked(result);
            }
        }).start();
    }

    private String displayName(Uri uri) {
        String name = null;
        try (Cursor c = getContentResolver().query(uri, null, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) name = c.getString(idx);
            }
        } catch (Exception e) {
            // use the fallback below
        }
        if (name == null || name.isEmpty()) name = "import_" + System.currentTimeMillis();
        return name.replaceAll("[\\\\/:*?\"<>|]", "_");
    }

    private String copyToImports(Uri uri) throws IOException {
        File dir = new File(dataDir, "imports");
        dir.mkdirs();
        File dst = new File(dir, displayName(uri));
        try (InputStream in = getContentResolver().openInputStream(uri); OutputStream out = new FileOutputStream(dst)) {
            if (in == null) throw new IOException("no access to the file");
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
        return dst.getAbsolutePath();
    }
}
