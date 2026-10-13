package com.diamon.tecladobios;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.text.TextUtils;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class MainActivity extends AppCompatActivity implements UsbSerialManager.UsbListener {

    private static final int REQUEST_STORAGE_PERMISSION = 1001;

    // UI References - Header
    private TextView tvUsbStatus;
    private ImageView ivUsbIcon;
    private View viewPcLinkLed;
    private TextView tvPcLinkStatus;
    private TextView tvLastKey;
    private Button btnToggleMode;
    private Button btnDownloadFirmware;

    // UI References - Mouse Panel
    private FrameLayout viewTouchpad;
    private Button btnMouseLeft;
    private Button btnMouseMiddle;
    private Button btnMouseRight;

    // UI References - BIOS & D-Pad
    private Button btnKeyDel, btnKeyF2, btnKeyF11, btnKeyF12, btnKeyF10, btnKeyEsc;
    private Button btnKeyEnter, btnKeyTab, btnKeySpace, btnKeyPlus, btnKeyMinus, btnKeyCad;
    private ImageButton btnDpadUp, btnDpadDown, btnDpadLeft, btnDpadRight;
    private Button btnDpadCenter;

    // UI References - Terminal
    private ScrollView scrollSerialLog;
    private TextView tvSerialLog;
    private EditText etManualCmd;
    private Button btnSendCmd;
    private ImageButton btnClearLog;

    // Controllers & State
    private UsbSerialManager usbManager;
    private Vibrator vibrator;
    private boolean isBiosMode = true; // true = BIOS Rápido, false = Texto Completo

    // Touchpad state
    private float lastTouchX = 0f;
    private float lastTouchY = 0f;
    private long touchDownTime = 0L;
    private boolean isDragging = false;
    private long lastMoveSentTime = 0L;
    private static final long MOUSE_THROTTLE_MS = 30L; // ~33 Hz throttle for 9600 baud

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        applyImmersiveFullscreen();
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);

        initViews();
        setupListeners();

        usbManager = new UsbSerialManager(this, this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveFullscreen();
        if (usbManager != null) {
            usbManager.register();
            usbManager.connect();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (usbManager != null) {
            usbManager.unregister();
        }
    }

    private void applyImmersiveFullscreen() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (getWindow().getInsetsController() != null) {
                getWindow().getInsetsController().hide(
                        WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                getWindow().getInsetsController().setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private void initViews() {
        // Header
        tvUsbStatus = findViewById(R.id.tvUsbStatus);
        ivUsbIcon = findViewById(R.id.ivUsbIcon);
        viewPcLinkLed = findViewById(R.id.viewPcLinkLed);
        tvPcLinkStatus = findViewById(R.id.tvPcLinkStatus);
        tvLastKey = findViewById(R.id.tvLastKey);
        btnToggleMode = findViewById(R.id.btnToggleMode);
        btnDownloadFirmware = findViewById(R.id.btnDownloadFirmware);

        // Mouse Panel
        viewTouchpad = findViewById(R.id.viewTouchpad);
        btnMouseLeft = findViewById(R.id.btnMouseLeft);
        btnMouseMiddle = findViewById(R.id.btnMouseMiddle);
        btnMouseRight = findViewById(R.id.btnMouseRight);

        // BIOS Keys
        btnKeyDel = findViewById(R.id.btnKeyDel);
        btnKeyF2 = findViewById(R.id.btnKeyF2);
        btnKeyF11 = findViewById(R.id.btnKeyF11);
        btnKeyF12 = findViewById(R.id.btnKeyF12);
        btnKeyF10 = findViewById(R.id.btnKeyF10);
        btnKeyEsc = findViewById(R.id.btnKeyEsc);
        btnKeyEnter = findViewById(R.id.btnKeyEnter);
        btnKeyTab = findViewById(R.id.btnKeyTab);
        btnKeySpace = findViewById(R.id.btnKeySpace);
        btnKeyPlus = findViewById(R.id.btnKeyPlus);
        btnKeyMinus = findViewById(R.id.btnKeyMinus);
        btnKeyCad = findViewById(R.id.btnKeyCad);

        // D-Pad
        btnDpadUp = findViewById(R.id.btnDpadUp);
        btnDpadDown = findViewById(R.id.btnDpadDown);
        btnDpadLeft = findViewById(R.id.btnDpadLeft);
        btnDpadRight = findViewById(R.id.btnDpadRight);
        btnDpadCenter = findViewById(R.id.btnDpadCenter);

        // Terminal
        scrollSerialLog = findViewById(R.id.scrollSerialLog);
        tvSerialLog = findViewById(R.id.tvSerialLog);
        etManualCmd = findViewById(R.id.etManualCmd);
        btnSendCmd = findViewById(R.id.btnSendCmd);
        btnClearLog = findViewById(R.id.btnClearLog);
    }

    private void setupListeners() {
        // Mode switch
        btnToggleMode.setOnClickListener(v -> toggleMode());

        // Firmware download
        btnDownloadFirmware.setOnClickListener(v -> checkPermissionAndExportFirmware());

        // Touchpad Listener
        setupTouchpad();

        // Mouse Buttons
        btnMouseLeft.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    triggerHaptic();
                    usbManager.sendMouseDown(UsbSerialManager.MOUSE_BUTTON_LEFT);
                    updateLastAction("CLICK IZQ (DOWN)");
                    return true;
                case MotionEvent.ACTION_UP:
                    usbManager.sendMouseUp(UsbSerialManager.MOUSE_BUTTON_LEFT);
                    updateLastAction("CLICK IZQ (UP)");
                    return true;
            }
            return false;
        });

        btnMouseMiddle.setOnClickListener(v -> {
            triggerHaptic();
            usbManager.sendMouseClick(UsbSerialManager.MOUSE_BUTTON_MIDDLE);
            updateLastAction("CLICK MEDIO");
        });

        btnMouseRight.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    triggerHaptic();
                    usbManager.sendMouseDown(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                    updateLastAction("CLICK DER (DOWN)");
                    return true;
                case MotionEvent.ACTION_UP:
                    usbManager.sendMouseUp(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                    updateLastAction("CLICK DER (UP)");
                    return true;
            }
            return false;
        });

        // Dedicated BIOS keys
        btnKeyDel.setOnClickListener(v -> sendKeyAction("DEL / SUPR", "s", "[DEL]"));
        btnKeyF2.setOnClickListener(v -> sendKeyAction("F2 SETUP", "2", "[F2]"));
        btnKeyF11.setOnClickListener(v -> sendKeyAction("F11 BOOT", "f", "[F11]"));
        btnKeyF12.setOnClickListener(v -> sendKeyAction("F12 BOOT", "b", "[F12]"));
        btnKeyF10.setOnClickListener(v -> sendKeyAction("F10 GUARDAR", "0", "[F10]"));
        btnKeyEsc.setOnClickListener(v -> sendKeyAction("ESC", "x", "[ESC]"));
        btnKeyEnter.setOnClickListener(v -> sendKeyAction("ENTER", "e", "[ENTER]"));
        btnKeyTab.setOnClickListener(v -> sendKeyAction("TAB", "\t", "[TAB]"));
        btnKeySpace.setOnClickListener(v -> sendKeyAction("ESPACIO", " ", " "));
        btnKeyPlus.setOnClickListener(v -> sendKeyAction("+", "+", "+"));
        btnKeyMinus.setOnClickListener(v -> sendKeyAction("-", "-", "-"));
        btnKeyCad.setOnClickListener(v -> sendKeyAction("CTRL+ALT+DEL", "[CAD]", "[CAD]"));

        // D-Pad
        btnDpadUp.setOnClickListener(v -> sendKeyAction("ARRIBA", "u", "[UP]"));
        btnDpadDown.setOnClickListener(v -> sendKeyAction("ABAJO", "d", "[DOWN]"));
        btnDpadLeft.setOnClickListener(v -> sendKeyAction("IZQUIERDA", "l", "[LEFT]"));
        btnDpadRight.setOnClickListener(v -> sendKeyAction("DERECHA", "r", "[RIGHT]"));
        btnDpadCenter.setOnClickListener(v -> sendKeyAction("OK / ENTER", "e", "[ENTER]"));

        // QWERTY Keys
        setupQwertyKeys();

        // Extra Diagnostic Buttons
        findViewById(R.id.btnTestHardware).setOnClickListener(v -> {
            triggerHaptic();
            usbManager.sendString("#TEST\n");
            updateLastAction("#TEST");
            logTerminalTx("#TEST");
        });

        findViewById(R.id.btnResetUsb).setOnClickListener(v -> {
            triggerHaptic();
            usbManager.sendString("#RESET\n");
            updateLastAction("#RESET");
            logTerminalTx("#RESET");
        });

        findViewById(R.id.btnQueryStatus).setOnClickListener(v -> {
            triggerHaptic();
            usbManager.sendString("?\n");
            updateLastAction("?");
            logTerminalTx("?");
        });

        // Terminal Actions
        btnClearLog.setOnClickListener(v -> tvSerialLog.setText(""));

        btnSendCmd.setOnClickListener(v -> sendManualCommand());
        etManualCmd.setOnEditorActionListener((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_SEND) {
                sendManualCommand();
                return true;
            }
            return false;
        });
    }

    private void setupTouchpad() {
        viewTouchpad.setOnTouchListener((v, event) -> {
            int action = event.getActionMasked();
            int pointerCount = event.getPointerCount();
            long now = System.currentTimeMillis();

            switch (action) {
                case MotionEvent.ACTION_DOWN:
                    lastTouchX = event.getX();
                    lastTouchY = event.getY();
                    touchDownTime = now;
                    isDragging = false;
                    return true;

                case MotionEvent.ACTION_MOVE:
                    float curX = event.getX();
                    float curY = event.getY();
                    float deltaX = curX - lastTouchX;
                    float deltaY = curY - lastTouchY;

                    // Check if finger moved sufficiently to count as mouse drag
                    if (Math.abs(deltaX) > 1.5f || Math.abs(deltaY) > 1.5f) {
                        isDragging = true;

                        if (now - lastMoveSentTime >= MOUSE_THROTTLE_MS) {
                            int dx = (int) (deltaX * 1.6f);
                            int dy = (int) (deltaY * 1.6f);
                            if (dx != 0 || dy != 0) {
                                usbManager.sendMouseMove(dx, dy);
                                lastMoveSentTime = now;
                            }
                            lastTouchX = curX;
                            lastTouchY = curY;
                        }
                    }
                    return true;

                case MotionEvent.ACTION_UP:
                    long duration = now - touchDownTime;
                    if (!isDragging && duration < 300) {
                        // Quick tap detected -> Left Click
                        triggerHaptic();
                        if (pointerCount == 2) {
                            usbManager.sendMouseClick(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                            updateLastAction("TAP CLICK DER");
                        } else {
                            usbManager.sendMouseClick(UsbSerialManager.MOUSE_BUTTON_LEFT);
                            updateLastAction("TAP CLICK IZQ");
                        }
                    }
                    return true;
            }
            return false;
        });
    }

    private void setupQwertyKeys() {
        // Numbers 1-0
        int[] numIds = {R.id.key_1, R.id.key_2, R.id.key_3, R.id.key_4, R.id.key_5,
                R.id.key_6, R.id.key_7, R.id.key_8, R.id.key_9, R.id.key_0};
        String[] numChars = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
        for (int i = 0; i < numIds.length; i++) {
            final String ch = numChars[i];
            findViewById(numIds[i]).setOnClickListener(v -> sendKeyAction(ch, ch, ch));
        }

        // QWERTY Letters
        int[] letterIds = {
                R.id.key_q, R.id.key_w, R.id.key_e, R.id.key_r, R.id.key_t,
                R.id.key_y, R.id.key_u, R.id.key_i, R.id.key_o, R.id.key_p,
                R.id.key_a, R.id.key_s, R.id.key_d, R.id.key_f, R.id.key_g,
                R.id.key_h, R.id.key_j, R.id.key_k, R.id.key_l, R.id.key_nn,
                R.id.key_z, R.id.key_x, R.id.key_c, R.id.key_v, R.id.key_b,
                R.id.key_n, R.id.key_m
        };
        String[] letters = {
                "q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
                "a", "s", "d", "f", "g", "h", "j", "k", "l", "ñ",
                "z", "x", "c", "v", "b", "n", "m"
        };
        for (int i = 0; i < letterIds.length; i++) {
            final String letter = letters[i];
            findViewById(letterIds[i]).setOnClickListener(v -> sendKeyAction(letter.toUpperCase(), letter, letter));
        }

        // Backspace
        findViewById(R.id.key_bs).setOnClickListener(v -> sendKeyAction("BORRAR", "\b", "[BS]"));
    }

    private void sendKeyAction(String displayName, String biosCmd, String textCmd) {
        triggerHaptic();
        updateLastAction(displayName);

        String cmdToSend = isBiosMode ? biosCmd : textCmd;
        usbManager.sendString(cmdToSend);
        logTerminalTx(cmdToSend);
    }

    private void toggleMode() {
        triggerHaptic();
        isBiosMode = !isBiosMode;
        if (isBiosMode) {
            btnToggleMode.setText(R.string.mode_bios);
            btnToggleMode.setTextColor(ContextCompat.getColor(this, R.color.bios_cyan));
            usbManager.sendString("#BIOS\n");
            logTerminalTx("#BIOS (Modo BIOS Rápido activado)");
        } else {
            btnToggleMode.setText(R.string.mode_text);
            btnToggleMode.setTextColor(ContextCompat.getColor(this, R.color.bios_amber));
            usbManager.sendString("#TEXTO\n");
            logTerminalTx("#TEXTO (Modo Texto Completo activado)");
        }
    }

    private void sendManualCommand() {
        String cmd = etManualCmd.getText().toString().trim();
        if (!TextUtils.isEmpty(cmd)) {
            triggerHaptic();
            usbManager.sendString(cmd + "\n");
            logTerminalTx(cmd);
            etManualCmd.setText("");
        }
    }

    private void updateLastAction(String actionName) {
        tvLastKey.setText(getString(R.string.last_key_format, actionName));
    }

    private void triggerHaptic() {
        if (vibrator != null && vibrator.hasVibrator()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator.vibrate(VibrationEffect.createOneShot(25, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                vibrator.vibrate(25);
            }
        }
    }

    private void logTerminalTx(String text) {
        appendLog("[TX] " + text);
    }

    private void appendLog(String message) {
        runOnUiThread(() -> {
            String timestamp = new SimpleDateFormat("HH:mm:ss", Locale.US).format(new Date());
            String newLine = "\n[" + timestamp + "] " + message;
            tvSerialLog.append(newLine);

            // Prune excess lines to prevent huge text memory consumption
            if (tvSerialLog.getText().length() > 6000) {
                String current = tvSerialLog.getText().toString();
                tvSerialLog.setText(current.substring(current.length() - 3000));
            }

            scrollSerialLog.post(() -> scrollSerialLog.fullScroll(View.FOCUS_DOWN));
        });
    }

    // --- USB SERIAL CALLBACKS ---

    @Override
    public void onUsbConnected(String deviceName) {
        tvUsbStatus.setText(getString(R.string.usb_connected) + " (" + deviceName + ")");
        tvUsbStatus.setTextColor(ContextCompat.getColor(this, R.color.status_green));
        ivUsbIcon.setColorFilter(ContextCompat.getColor(this, R.color.status_green));
        appendLog("[USB] Conectado exitosamente: " + deviceName + " a 9600 baud.");
        Toast.makeText(this, "Arduino Conectado (" + deviceName + ")", Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onUsbDisconnected() {
        tvUsbStatus.setText(R.string.usb_disconnected);
        tvUsbStatus.setTextColor(ContextCompat.getColor(this, R.color.status_red));
        ivUsbIcon.setColorFilter(ContextCompat.getColor(this, R.color.text_muted));

        onPcLinkChanged(false, "Desconectado");
        appendLog("[USB] Dispositivo USB desconectado.");
    }

    @Override
    public void onPcLinkChanged(boolean isLinked, String details) {
        if (isLinked) {
            tvPcLinkStatus.setText(R.string.pc_link_connected);
            tvPcLinkStatus.setTextColor(ContextCompat.getColor(this, R.color.status_green));
            setLedColor(ContextCompat.getColor(this, R.color.status_green));
            appendLog("[PC LINK] " + details);
        } else {
            tvPcLinkStatus.setText(R.string.pc_link_disconnected);
            tvPcLinkStatus.setTextColor(ContextCompat.getColor(this, R.color.status_red));
            setLedColor(ContextCompat.getColor(this, R.color.status_red));
        }
    }

    private void setLedColor(int color) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.OVAL);
        drawable.setColor(color);
        drawable.setStroke(2, Color.WHITE);
        viewPcLinkLed.setBackground(drawable);
    }

    @Override
    public void onDataReceived(String data) {
        appendLog("[RX] " + data.trim());
    }

    @Override
    public void onError(String message) {
        appendLog("[ERROR] " + message);
    }

    // --- FIRMWARE EXPORT HANDLER ---

    private void checkPermissionAndExportFirmware() {
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE)
                    != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                        REQUEST_STORAGE_PERMISSION);
                return;
            }
        }
        executeFirmwareExport();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_STORAGE_PERMISSION) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                executeFirmwareExport();
            } else {
                Toast.makeText(this, "Permiso de almacenamiento requerido para exportar.", Toast.LENGTH_LONG).show();
            }
        }
    }

    private void executeFirmwareExport() {
        Toast.makeText(this, R.string.firmware_exporting, Toast.LENGTH_SHORT).show();
        appendLog("[FIRMWARE] Iniciando extracción de firmware a descargas...");

        FirmwareExporter.exportFirmwareAsync(this, new FirmwareExporter.ExportCallback() {
            @Override
            public void onSuccess(String targetDirectory, List<String> exportedFiles) {
                runOnUiThread(() -> {
                    String msg = "Firmware exportado a: " + targetDirectory + " (" + exportedFiles.size() + " archivos)";
                    Toast.makeText(MainActivity.this, msg, Toast.LENGTH_LONG).show();
                    appendLog("[FIRMWARE] " + msg);
                    for (String f : exportedFiles) {
                        appendLog("  - " + f);
                    }
                });
            }

            @Override
            public void onError(String errorMessage) {
                runOnUiThread(() -> {
                    String err = "Error al exportar: " + errorMessage;
                    Toast.makeText(MainActivity.this, err, Toast.LENGTH_LONG).show();
                    appendLog("[FIRMWARE ERROR] " + err);
                });
            }
        });
    }
}
