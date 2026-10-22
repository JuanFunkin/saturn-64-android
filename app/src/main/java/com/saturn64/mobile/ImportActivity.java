package com.saturn64.mobile;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.util.zip.CRC32;

/**
 * Start screen: Saturn background + logo, "Import ROM" opens the phone's file picker.
 * The ROM is copied to the app's internal folder as "sm64.z64" (the name expected by
 * saturn_rom_extract.cpp) and verified (size + CRC32), exactly like Saturn does on PC.
 */
public class ImportActivity extends Activity {

    private static final int REQ_PICK_ROM = 1001;
    private static final int REQ_SAVE_LOG = 1002;
    private static final long ROM_SIZE = 8L * 1024 * 1024;
    private static final long ROM_CRC32 = 0x3CE60709L;

    private TextView status;
    private Button importButton;
    private Button continueButton;

    private File romFile() {
        return new File(getFilesDir(), "sm64.z64");
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private Button makeButton(String text) {
        Button b = new Button(this);
        b.setText(text);
        b.setAllCaps(false);
        b.setTextColor(Color.WHITE);
        b.setTextSize(17);
        b.setTypeface(Typeface.DEFAULT_BOLD);
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0x55FFFFFF);
        bg.setCornerRadius(dp(14));
        bg.setStroke(dp(2), 0xCCFFFFFF);
        b.setBackground(bg);
        b.setMinWidth(dp(190));
        b.setMinimumHeight(dp(52));
        b.setPadding(dp(18), dp(8), dp(18), dp(8));
        return b;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.BLACK);

        // Saturn background + logo
        ImageView bg = new ImageView(this);
        bg.setImageResource(R.drawable.saturn_bg);
        bg.setScaleType(ImageView.ScaleType.CENTER_CROP);
        root.addView(bg, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));

        // Dark gradient at the bottom so the text and buttons are readable
        View shade = new View(this);
        GradientDrawable g = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] { 0x00000000, 0xE6000000 });
        shade.setBackground(g);
        FrameLayout.LayoutParams shadeLp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, 0, Gravity.BOTTOM);
        shadeLp.height = Math.round(getResources().getDisplayMetrics().heightPixels * 0.42f);
        root.addView(shade, shadeLp);

        // Status text + buttons
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER_HORIZONTAL);
        box.setPadding(dp(24), dp(8), dp(24), dp(22));

        status = new TextView(this);
        status.setTextColor(Color.WHITE);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setShadowLayer(6f, 0f, 2f, Color.BLACK);
        LinearLayout.LayoutParams stLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        stLp.bottomMargin = dp(12);
        box.addView(status, stLp);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER);

        importButton = makeButton("Import ROM");
        importButton.setOnClickListener(v -> openPicker());
        LinearLayout.LayoutParams bLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        bLp.rightMargin = dp(10);
        row.addView(importButton, bLp);

        continueButton = makeButton("Continue");
        continueButton.setOnClickListener(v -> startEngine());
        LinearLayout.LayoutParams cLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        cLp.leftMargin = dp(10);
        row.addView(continueButton, cLp);

        box.addView(row);

        Button logButton = new Button(this);
        logButton.setText("Save debug log");
        logButton.setAllCaps(false);
        logButton.setTextColor(0xCCFFFFFF);
        logButton.setTextSize(13);
        logButton.setBackgroundColor(Color.TRANSPARENT);
        logButton.setOnClickListener(v -> saveLog());
        LinearLayout.LayoutParams lgLp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lgLp.topMargin = dp(6);
        box.addView(logButton, lgLp);
        root.addView(box, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT, Gravity.BOTTOM));

        setContentView(root);
        refreshUi();
    }

    private void setEnabledLook(Button b, boolean enabled) {
        b.setEnabled(enabled);
        b.setAlpha(enabled ? 1f : 0.45f);
    }

    private void refreshUi() {
        File f = romFile();
        boolean ready = f.exists() && f.length() == ROM_SIZE;
        if (ready) {
            status.setText("ROM ready.");
            importButton.setText("Import another ROM");
        } else {
            status.setText("Import your Super Mario 64 ROM (US, .z64) to get started.");
            importButton.setText("Import ROM");
        }
        setEnabledLook(importButton, true);
        setEnabledLook(continueButton, ready);
    }

    private void openPicker() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, REQ_PICK_ROM);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQ_SAVE_LOG) {
            if (resultCode == RESULT_OK && data != null && data.getData() != null) writeLog(data.getData());
            return;
        }
        if (requestCode != REQ_PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        final Uri uri = data.getData();
        status.setText("Importing and verifying...");
        setEnabledLook(importButton, false);
        setEnabledLook(continueButton, false);
        new Thread(() -> {
            final String error = importRom(uri);
            runOnUiThread(() -> {
                if (error != null) {
                    Toast.makeText(this, error, Toast.LENGTH_LONG).show();
                }
                refreshUi();
            });
        }).start();
    }

    /** Returns null on success, or the error message. */
    private String importRom(Uri uri) {
        File tmp = new File(getFilesDir(), "sm64.import.tmp");
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(tmp)) {
            if (in == null) return "Could not open the file.";
            CRC32 crc = new CRC32();
            byte[] buf = new byte[64 * 1024];
            long total = 0;
            int n;
            while ((n = in.read(buf)) > 0) {
                total += n;
                if (total > ROM_SIZE) {
                    tmp.delete();
                    return "Invalid ROM: the file is too large (it must be 8 MB).";
                }
                crc.update(buf, 0, n);
                out.write(buf, 0, n);
            }
            out.flush();
            if (total != ROM_SIZE) {
                tmp.delete();
                return "Invalid ROM: the size is not 8 MB.";
            }
            if (crc.getValue() != ROM_CRC32) {
                tmp.delete();
                return "Invalid ROM: use an unmodified US ROM in .z64 format.";
            }
        } catch (IOException | SecurityException e) {
            tmp.delete();
            return "Error copying the ROM: " + e.getMessage();
        }
        File dst = romFile();
        if (dst.exists() && !dst.delete()) {
            tmp.delete();
            return "Could not replace the previous ROM.";
        }
        if (!tmp.renameTo(dst)) {
            tmp.delete();
            return "Could not save the ROM.";
        }
        return null;
    }

    private void startEngine() {
        status.setText("Preparing Saturn files...");
        setEnabledLook(importButton, false);
        setEnabledLook(continueButton, false);
        new Thread(() -> {
            String error = null;
            try {
                AssetInstaller.install(getAssets(), getFilesDir());
            } catch (IOException e) {
                error = "Error preparing the files: " + e.getMessage();
            }
            final String err = error;
            runOnUiThread(() -> {
                if (err != null) {
                    Toast.makeText(this, err, Toast.LENGTH_LONG).show();
                } else {
                    startActivity(new Intent(this, SaturnActivity.class));
                }
                refreshUi();
            });
        }).start();
    }

    // ---- Debug log (so crashes can be shared) ----

    private void saveLog() {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_TITLE, "saturn_log.txt");
        startActivityForResult(i, REQ_SAVE_LOG);
    }

    private static String tail(File f, int maxBytes) {
        if (!f.exists()) return "(missing: " + f.getName() + ")\n";
        try (RandomAccessFile raf = new RandomAccessFile(f, "r")) {
            long len = raf.length();
            int n = (int) Math.min(len, maxBytes);
            byte[] b = new byte[n];
            raf.seek(len - n);
            raf.readFully(b);
            return new String(b, "UTF-8");
        } catch (IOException e) {
            return "(could not read " + f.getName() + ": " + e.getMessage() + ")\n";
        }
    }

    private void writeLog(Uri uri) {
        StringBuilder sb = new StringBuilder();
        sb.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append('\n');
        sb.append("Android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
        sb.append("ABIs: ").append(TextUtils.join(", ", Build.SUPPORTED_ABIS)).append("\n\n");
        sb.append("===== crash.txt =====\n").append(tail(new File(getFilesDir(), "crash.txt"), 20000));
        sb.append("\n===== saturn_log.txt (last run) =====\n").append(tail(new File(getFilesDir(), "saturn_log.txt"), 150000));
        sb.append("\n===== saturn_log_prev.txt (run before) =====\n").append(tail(new File(getFilesDir(), "saturn_log_prev.txt"), 40000));
        try (OutputStream out = getContentResolver().openOutputStream(uri)) {
            if (out == null) throw new IOException("no output stream");
            out.write(sb.toString().getBytes("UTF-8"));
            Toast.makeText(this, "Log saved.", Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            Toast.makeText(this, "Could not save the log: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }
}
