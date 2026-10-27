package com.saturnstudio;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
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
 * Start screen.
 *
 *  - Saturn Studio keeps EVERYTHING (projects, exported frames, models, config, logs) in
 *    /storage/emulated/0/com.saturnstudio. That needs the "All files access" permission.
 *    If the user refuses, the app falls back to its private storage.
 *  - "Import ROM" opens the phone's file picker; the ROM is verified (8 MB + CRC32 of the US ROM) and copied
 *    to <data>/rom.z64. The engine extracts the assets from it on the first run.
 *  - Once the assets are extracted (marker res/.extracted written by the engine) this screen is skipped and the
 *    editor starts directly. If the last run crashed, the screen is shown so the log can be shared.
 */
public class ImportActivity extends Activity {

    private static final int REQ_PICK_ROM = 1001;
    private static final int REQ_SAVE_LOG = 1002;
    private static final int REQ_STORAGE = 1003;
    private static final long ROM_SIZE = 8L * 1024 * 1024;
    private static final long ROM_CRC32 = 0x3CE60709L;
    private static final String DATA_FOLDER = "com.saturnstudio";

    private TextView status;
    private Button importButton;
    private Button continueButton;
    private Button shareButton;

    private boolean busy = false;
    private boolean launching = false;
    private boolean noAutoLaunch = false;

    // ------------------------------------------------------------------ storage

    private SharedPreferences prefs() {
        return getSharedPreferences("saturnstudio", MODE_PRIVATE);
    }

    private boolean usePrivate() {
        return prefs().getBoolean("private_storage", false);
    }

    private boolean hasExternalAccess() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        if (Build.VERSION.SDK_INT >= 23) {
            return checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
        }
        return true;
    }

    private boolean storageReady() {
        return usePrivate() || hasExternalAccess();
    }

    private File dataDir() {
        if (!usePrivate() && hasExternalAccess()) {
            return new File(Environment.getExternalStorageDirectory(), DATA_FOLDER);
        }
        return getFilesDir();
    }

    /** Creates the data folder and tells the engine where it is (datadir.txt). */
    private void prepareDataDir() {
        File d = dataDir();
        d.mkdirs();
        if (!d.isDirectory() || !d.canWrite()) {
            prefs().edit().putBoolean("private_storage", true).apply();
            d = getFilesDir();
        }
        try (OutputStream out = new FileOutputStream(new File(getFilesDir(), "datadir.txt"))) {
            out.write(d.getAbsolutePath().getBytes("UTF-8"));
        } catch (IOException e) {
            // the engine falls back to the internal folder
        }
    }

    private void requestStorageAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            try {
                startActivityForResult(i, REQ_STORAGE);
            } catch (Exception e) {
                startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION), REQ_STORAGE);
            }
        } else if (Build.VERSION.SDK_INT >= 23) {
            requestPermissions(new String[] { Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    Manifest.permission.READ_EXTERNAL_STORAGE }, REQ_STORAGE);
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        refreshUi();
    }

    private File romFile() {
        return new File(dataDir(), "rom.z64");
    }

    private File extractedMarker() {
        return new File(new File(dataDir(), "res"), ".extracted");
    }

    // ------------------------------------------------------------------ UI

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
        b.setMinimumHeight(dp(56));
        b.setPadding(dp(18), dp(8), dp(18), dp(8));
        return b;
    }

    private LinearLayout.LayoutParams wrap(int left, int right) {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = left;
        lp.rightMargin = right;
        return lp;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        FrameLayout root = new FrameLayout(this);
        GradientDrawable bg = new GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                new int[] { 0xFF0B0B2B, 0xFF1B1B4B, 0xFF000010 });
        root.setBackground(bg);

        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        box.setPadding(dp(24), dp(12), dp(24), dp(12));

        TextView title = new TextView(this);
        title.setText("Saturn Studio");
        title.setTextColor(Color.WHITE);
        title.setTextSize(34);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        box.addView(title, wrap(0, 0));

        status = new TextView(this);
        status.setTextColor(0xFFE0E0F0);
        status.setTextSize(15);
        status.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams stLp = wrap(0, 0);
        stLp.topMargin = dp(10);
        stLp.bottomMargin = dp(18);
        box.addView(status, stLp);

        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER);

        importButton = makeButton("Import ROM");
        importButton.setOnClickListener(v -> onImportClicked());
        row.addView(importButton, wrap(0, dp(10)));

        continueButton = makeButton("Continue");
        continueButton.setOnClickListener(v -> onContinueClicked());
        row.addView(continueButton, wrap(dp(10), dp(10)));

        shareButton = makeButton("Share log");
        shareButton.setOnClickListener(v -> shareLog());
        row.addView(shareButton, wrap(dp(10), 0));
        shareButton.setVisibility(View.GONE);

        box.addView(row);

        Button logButton = new Button(this);
        logButton.setText("Save debug log");
        logButton.setAllCaps(false);
        logButton.setTextColor(0xCCFFFFFF);
        logButton.setTextSize(13);
        logButton.setBackgroundColor(Color.TRANSPARENT);
        logButton.setOnClickListener(v -> saveLog());
        LinearLayout.LayoutParams lgLp = wrap(0, 0);
        lgLp.topMargin = dp(8);
        box.addView(logButton, lgLp);

        root.addView(box, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        setContentView(root);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshUi();
    }

    private void setEnabledLook(Button b, boolean enabled) {
        b.setEnabled(enabled);
        b.setAlpha(enabled ? 1f : 0.45f);
    }

    private String romError() {
        File f = new File(dataDir(), "rom_error.txt");
        if (!f.exists()) return null;
        String t = tail(f, 100).trim();
        f.delete();
        if (t.equals("rom_missing")) return "The ROM is missing. Import it again.";
        return "The ROM is not valid. Use an unmodified Super Mario 64 (US) ROM in .z64 format.";
    }

    private void refreshUi() {
        if (launching || busy) return;

        if (!storageReady()) {
            status.setText("Saturn Studio keeps everything (projects, exported frames, models, settings) in the folder\n"
                    + "/storage/emulated/0/" + DATA_FOLDER + "\n\nAllow access to all files to continue.");
            importButton.setText("Grant access");
            continueButton.setText("Use private storage");
            setEnabledLook(importButton, true);
            setEnabledLook(continueButton, true);
            return;
        }

        prepareDataDir();
        File crash = new File(dataDir(), "crash.txt");
        boolean abnormal = new File(getFilesDir(), "running.flag").exists();
        boolean extracted = extractedMarker().exists();
        String romProblem = romError();
        if (romProblem != null) {
            extractedMarker().delete();
            extracted = false;
        }

        if (extracted && !crash.exists() && !abnormal && !noAutoLaunch) {
            startEngine();
            return;
        }

        File rom = romFile();
        boolean romReady = rom.exists() && rom.length() == ROM_SIZE;
        boolean canContinue = romReady || extracted;

        boolean problem = crash.exists() || abnormal;
        shareButton.setVisibility(problem ? View.VISIBLE : View.GONE);
        if (romProblem != null) {
            status.setText(romProblem);
        } else if (problem) {
            status.setText("Saturn Studio did not close normally last time. Tap \"Share log\" to send the log to the developer, or Continue to try again.");
        } else if (canContinue) {
            status.setText("ROM ready. Your data is stored in /storage/emulated/0/" + DATA_FOLDER);
        } else {
            status.setText("Import your Super Mario 64 ROM (US, .z64) to get started.\nIt is copied to the " + DATA_FOLDER + " folder and the assets are extracted from it.");
        }
        importButton.setText(canContinue ? "Import another ROM" : "Import ROM");
        continueButton.setText("Continue");
        setEnabledLook(importButton, true);
        setEnabledLook(continueButton, canContinue);
    }

    private void onImportClicked() {
        if (!storageReady()) requestStorageAccess();
        else openPicker();
    }

    private void onContinueClicked() {
        if (!storageReady()) {
            prefs().edit().putBoolean("private_storage", true).apply();
            refreshUi();
        } else {
            startEngine();
        }
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
        if (requestCode != REQ_PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null) return;
        final Uri uri = data.getData();
        busy = true;
        status.setText("Importing and verifying the ROM...");
        setEnabledLook(importButton, false);
        setEnabledLook(continueButton, false);
        new Thread(() -> {
            final String error = importRom(uri);
            runOnUiThread(() -> {
                busy = false;
                if (error != null) Toast.makeText(this, error, Toast.LENGTH_LONG).show();
                else new File(dataDir(), "res/.extracted").delete(); // new ROM: extract again
                noAutoLaunch = true;
                refreshUi();
            });
        }).start();
    }

    /** Returns null on success, or the error message. */
    private String importRom(Uri uri) {
        File dir = dataDir();
        dir.mkdirs();
        File tmp = new File(dir, "rom.import.tmp");
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
                return "Invalid ROM: use an unmodified US ROM in .z64 format (not .n64 or .v64).";
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
        if (launching) return;
        launching = true;
        busy = true;
        status.setText("Preparing Saturn Studio files...");
        setEnabledLook(importButton, false);
        setEnabledLook(continueButton, false);
        final File dir = dataDir();
        new Thread(() -> {
            String error = null;
            try {
                AssetInstaller.install(getAssets(), dir);
                new File(dir, "crash.txt").delete();
                new File(dir, "ready.flag").delete();
                new File(dir, "extract_status.txt").delete();
                new File(getFilesDir(), "running.flag").delete();
            } catch (IOException e) {
                error = "Error preparing the files: " + e.getMessage();
            }
            final String err = error;
            runOnUiThread(() -> {
                if (err != null) {
                    launching = false;
                    busy = false;
                    noAutoLaunch = true;
                    Toast.makeText(this, err, Toast.LENGTH_LONG).show();
                    refreshUi();
                } else {
                    startActivity(new Intent(this, SaturnActivity.class));
                    finish();
                }
            });
        }).start();
    }

    // ------------------------------------------------------------------ debug log

    private void saveLog() {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("text/plain");
        i.putExtra(Intent.EXTRA_TITLE, "saturnstudio_log.txt");
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

    private String buildLogText(int logBytes, int crashBytes) {
        File dir = dataDir();
        StringBuilder sb = new StringBuilder();
        sb.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL).append('\n');
        sb.append("Android: ").append(Build.VERSION.RELEASE).append(" (API ").append(Build.VERSION.SDK_INT).append(")\n");
        sb.append("ABIs: ").append(TextUtils.join(", ", Build.SUPPORTED_ABIS)).append('\n');
        sb.append("Data folder: ").append(dir.getAbsolutePath()).append("\n\n");
        sb.append("===== crash.txt =====\n").append(tail(new File(dir, "crash.txt"), crashBytes));
        sb.append("\n===== extract_status.txt =====\n").append(tail(new File(dir, "extract_status.txt"), 300));
        sb.append("\n===== stderr.log =====\n").append(tail(new File(dir, "stderr.log"), logBytes));
        sb.append("\n===== latest.log =====\n").append(tail(new File(dir, "latest.log"), logBytes));
        return sb.toString();
    }

    private void writeLog(Uri uri) {
        String text = buildLogText(120000, 20000);
        try (OutputStream out = getContentResolver().openOutputStream(uri)) {
            if (out == null) throw new IOException("no output stream");
            out.write(text.getBytes("UTF-8"));
            Toast.makeText(this, "Log saved.", Toast.LENGTH_SHORT).show();
        } catch (IOException e) {
            Toast.makeText(this, "Could not save the log: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void shareLog() {
        String text = buildLogText(30000, 8000);
        Intent send = new Intent(Intent.ACTION_SEND);
        send.setType("text/plain");
        send.putExtra(Intent.EXTRA_SUBJECT, "Saturn Studio Android log");
        send.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(send, "Share log"));
    }
}
