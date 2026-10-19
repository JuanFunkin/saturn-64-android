package com.saturn64.mobile;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.CRC32;

/**
 * Pantalla de inicio: boton "Importar ROM" que abre el explorador de archivos del telefono.
 * Copia el ROM a la carpeta interna de la app como "sm64.z64" (el nombre que espera
 * saturn_rom_extract.cpp) y comprueba tamano y CRC32, igual que Saturn en PC.
 */
public class ImportActivity extends Activity {

    private static final int REQ_PICK_ROM = 1001;
    private static final long ROM_SIZE = 8L * 1024 * 1024;   // mismo valor que Saturn
    private static final long ROM_CRC32 = 0x3CE60709L;        // mismo valor que Saturn (US)

    private TextView status;
    private Button importButton;
    private Button continueButton;

    private File romFile() {
        return new File(getFilesDir(), "sm64.z64");
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(dp(24), dp(24), dp(24), dp(24));

        TextView title = new TextView(this);
        title.setText("Saturn 64");
        title.setTextSize(28);
        title.setGravity(Gravity.CENTER);
        root.addView(title);

        status = new TextView(this);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0, dp(16), 0, dp(24));
        root.addView(status);

        importButton = new Button(this);
        importButton.setOnClickListener(v -> openPicker());
        root.addView(importButton);

        continueButton = new Button(this);
        continueButton.setText("Continuar");
        continueButton.setOnClickListener(v -> startEngine());
        root.addView(continueButton);

        setContentView(root);
        refreshUi();
    }

    private void refreshUi() {
        File f = romFile();
        boolean ready = f.exists() && f.length() == ROM_SIZE;
        if (ready) {
            status.setText("ROM lista.");
            importButton.setText("Importar otro ROM");
        } else {
            status.setText("Importa tu ROM de Super Mario 64 (US, .z64) para empezar.");
            importButton.setText("Importar ROM");
        }
        continueButton.setEnabled(ready);
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
        if (requestCode != REQ_PICK_ROM || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        final Uri uri = data.getData();
        status.setText("Importando y verificando...");
        importButton.setEnabled(false);
        continueButton.setEnabled(false);
        new Thread(() -> {
            final String error = importRom(uri);
            runOnUiThread(() -> {
                importButton.setEnabled(true);
                if (error != null) {
                    Toast.makeText(this, error, Toast.LENGTH_LONG).show();
                }
                refreshUi();
            });
        }).start();
    }

    /** Devuelve null si todo salio bien, o el mensaje de error. */
    private String importRom(Uri uri) {
        File tmp = new File(getFilesDir(), "sm64.import.tmp");
        try (InputStream in = getContentResolver().openInputStream(uri);
             OutputStream out = new FileOutputStream(tmp)) {
            if (in == null) return "No se pudo abrir el archivo.";
            CRC32 crc = new CRC32();
            byte[] buf = new byte[64 * 1024];
            long total = 0;
            int n;
            while ((n = in.read(buf)) > 0) {
                total += n;
                if (total > ROM_SIZE) {
                    tmp.delete();
                    return "ROM invalido: el archivo es demasiado grande (debe pesar 8 MB).";
                }
                crc.update(buf, 0, n);
                out.write(buf, 0, n);
            }
            out.flush();
            if (total != ROM_SIZE) {
                tmp.delete();
                return "ROM invalido: el tamano no es 8 MB.";
            }
            if (crc.getValue() != ROM_CRC32) {
                tmp.delete();
                return "ROM invalido: usa un ROM US sin modificar en formato .z64.";
            }
        } catch (IOException | SecurityException e) {
            tmp.delete();
            return "Error al copiar el ROM: " + e.getMessage();
        }
        File dst = romFile();
        if (dst.exists() && !dst.delete()) {
            tmp.delete();
            return "No se pudo reemplazar el ROM anterior.";
        }
        if (!tmp.renameTo(dst)) {
            tmp.delete();
            return "No se pudo guardar el ROM.";
        }
        return null;
    }

    private void startEngine() {
        status.setText("Preparando archivos de Saturn...");
        importButton.setEnabled(false);
        continueButton.setEnabled(false);
        new Thread(() -> {
            String error = null;
            try {
                AssetInstaller.install(getAssets(), getFilesDir());
            } catch (java.io.IOException e) {
                error = "Error al preparar los archivos: " + e.getMessage();
            }
            final String err = error;
            runOnUiThread(() -> {
                importButton.setEnabled(true);
                if (err != null) {
                    Toast.makeText(this, err, Toast.LENGTH_LONG).show();
                    refreshUi();
                } else {
                    startActivity(new Intent(this, SaturnActivity.class));
                    refreshUi();
                }
            });
        }).start();
    }
}
