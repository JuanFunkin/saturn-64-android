package com.saturn64.mobile;

import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.widget.RelativeLayout;
import android.widget.TextView;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

/**
 * Starts SDL2 and loads libSDL2.so + libmain.so (the Saturn engine).
 *
 * - Shows a loading overlay while Saturn starts / extracts the ROM assets (the engine writes its progress to
 *   "extract_status.txt" in the data folder). On slow phones the first start can take minutes and the screen
 *   would otherwise look black.
 * - Creates "running.flag" at start and removes it on a normal exit, so the launcher can tell when the last
 *   run ended abnormally (crash / forced close) and offer to share the log.
 * - openFolder(): called from the engine (JNI) by the "Open ... folder" buttons.
 */
public class SaturnActivity extends SDLActivity {

    private TextView overlay;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private File dataDir;
    private long overlayShownAt = 0;
    private String lastStatus = "";
    private long lastStatusChange = 0;

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
        overlay.setText("Starting Saturn...");
        mLayout.addView(overlay, new RelativeLayout.LayoutParams(
                RelativeLayout.LayoutParams.MATCH_PARENT, RelativeLayout.LayoutParams.MATCH_PARENT));
        overlayShownAt = System.currentTimeMillis();
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
                        int pct = Integer.parseInt(first == null ? "0" : first.trim());
                        if (pct < 0) {
                            hide = true;
                        } else {
                            text = (msg == null || msg.isEmpty() ? "Loading..." : msg) + "\n" + pct + "%";
                        }
                    } catch (IOException | NumberFormatException e) {
                        // the engine is rewriting the file: try again next time
                    }
                } else {
                    text = "Starting Saturn...";
                }
                long now = System.currentTimeMillis();
                if (text != null && !text.equals(lastStatus)) {
                    lastStatus = text;
                    lastStatusChange = now;
                }
                if (hide) {
                    overlay.setVisibility(android.view.View.GONE);
                } else if (text != null) {
                    overlay.setVisibility(android.view.View.VISIBLE);
                    if (now - lastStatusChange > 45000) {
                        text += "\n\nThis is taking a long time. If the screen stays like this, close the app and open it"
                                + " again: it will offer to share the log.";
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
    }

    /** Called from native code (JNI): open a folder of the data directory in the phone's file manager. */
    public void openFolder(final String path) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    String root = Environment.getExternalStorageDirectory().getAbsolutePath();
                    if (!path.startsWith(root)) throw new IllegalArgumentException("not in shared storage");
                    String rel = path.substring(root.length());
                    while (rel.startsWith("/")) rel = rel.substring(1);
                    while (rel.endsWith("/")) rel = rel.substring(0, rel.length() - 1);
                    Uri uri = DocumentsContract.buildDocumentUri("com.android.externalstorage.documents", "primary:" + rel);
                    Intent i = new Intent(Intent.ACTION_VIEW);
                    i.setDataAndType(uri, "vnd.android.document/directory");
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(i);
                } catch (Exception e) {
                    Toast.makeText(SaturnActivity.this,
                            "Open this folder with your file manager:\n" + path, Toast.LENGTH_LONG).show();
                }
            }
        });
    }
}
