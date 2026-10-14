package com.diamon.tecladobios;

import android.Manifest;
import android.content.ClipData;
import android.content.ClipboardManager;
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
    private Button btnKeyF1, btnKeyF9, btnKeyPgUp, btnKeyPgDn;
    private ImageButton btnDpadUp, btnDpadDown, btnDpadLeft, btnDpadRight;
    private Button btnDpadCenter;

    // UI References - Terminal
    private ScrollView scrollSerialLog;
    private TextView tvSerialLog;
    private EditText etManualCmd;
    private Button btnSendCmd;
    private ImageButton btnClearLog;
    private ImageButton btnCopyLog;

    // Controllers & State
    private UsbSerialManager usbManager;
    private Vibrator vibrator;
    private boolean isBiosMode = true; // true = BIOS Rápido, false = Texto Completo

    // Touchpad state
    private float lastTouchX = 0f;
    private float lastTouchY = 0f;
    private long touchDownTime = 0L;
    private boolean isDragging = false;
    private int maxPointerCount = 1;
    private long lastMoveSentTime = 0L;
    private static final long MOUSE_THROTTLE_MS = 25L; // ~40 Hz throttle for smooth responsive tracking

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
        btnCopyLog = findViewById(R.id.btnCopyLog);

        // Extra Function & Nav Keys
        btnKeyF1 = findViewById(R.id.btnKeyF1);
        btnKeyF9 = findViewById(R.id.btnKeyF9);
        btnKeyPgUp = findViewById(R.id.btnKeyPgUp);
        btnKeyPgDn = findViewById(R.id.btnKeyPgDn);
    }

    private void setupListeners() {
        // Mode switch
        btnToggleMode.setOnClickListener(v -> toggleMode());

        // Firmware download
        btnDownloadFirmware.setOnClickListener(v -> checkPermissionAndExportFirmware());

        // Touchpad Listener
        setupTouchpad();

        // Mouse Buttons (Down, Up, Cancel, and Visual State)
        btnMouseLeft.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    triggerHaptic();
                    usbManager.sendMouseDown(UsbSerialManager.MOUSE_BUTTON_LEFT);
                    updateLastAction(getString(R.string.action_mouse_left_down));
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    usbManager.sendMouseUp(UsbSerialManager.MOUSE_BUTTON_LEFT);
                    updateLastAction(getString(R.string.action_mouse_left_up));
                    return true;
            }
            return false;
        });

        btnMouseMiddle.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    triggerHaptic();
                    usbManager.sendMouseDown(UsbSerialManager.MOUSE_BUTTON_MIDDLE);
                    updateLastAction(getString(R.string.action_mouse_mid_down));
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    usbManager.sendMouseUp(UsbSerialManager.MOUSE_BUTTON_MIDDLE);
                    updateLastAction(getString(R.string.action_mouse_mid_up));
                    return true;
            }
            return false;
        });

        btnMouseRight.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    v.setPressed(true);
                    triggerHaptic();
                    usbManager.sendMouseDown(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                    updateLastAction(getString(R.string.action_mouse_right_down));
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    v.setPressed(false);
                    usbManager.sendMouseUp(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                    updateLastAction(getString(R.string.action_mouse_right_up));
                    return true;
            }
            return false;
        });

        // Dedicated BIOS keys
        btnKeyDel.setOnClickListener(v -> sendKeyAction(getString(R.string.action_del), "s", "[DEL]"));
        btnKeyF2.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f2), "2", "[F2]"));
        btnKeyF11.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f11), "f", "[F11]"));
        btnKeyF12.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f12), "b", "[F12]"));
        btnKeyF10.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f10), "0", "[F10]"));
        btnKeyEsc.setOnClickListener(v -> sendKeyAction(getString(R.string.action_esc), "x", "[ESC]"));
        btnKeyEnter.setOnClickListener(v -> sendKeyAction(getString(R.string.action_enter), "e", "[ENTER]"));
        btnKeyTab.setOnClickListener(v -> sendKeyAction(getString(R.string.action_tab), "\t", "[TAB]"));
        btnKeySpace.setOnClickListener(v -> sendKeyAction(getString(R.string.action_space), " ", " "));
        btnKeyPlus.setOnClickListener(v -> sendKeyAction("+", "+", "+"));
        btnKeyMinus.setOnClickListener(v -> sendKeyAction("-", "-", "-"));
        btnKeyCad.setOnClickListener(v -> sendKeyAction(getString(R.string.key_cad), "[CAD]", "[CAD]"));

        // D-Pad
        btnDpadUp.setOnClickListener(v -> sendKeyAction(getString(R.string.action_dpad_up), "u", "[UP]"));
        btnDpadDown.setOnClickListener(v -> sendKeyAction(getString(R.string.action_dpad_down), "d", "[DOWN]"));
        btnDpadLeft.setOnClickListener(v -> sendKeyAction(getString(R.string.action_dpad_left), "l", "[LEFT]"));
        btnDpadRight.setOnClickListener(v -> sendKeyAction(getString(R.string.action_dpad_right), "r", "[RIGHT]"));
        btnDpadCenter.setOnClickListener(v -> sendKeyAction(getString(R.string.action_dpad_ok), "e", "[ENTER]"));

        // Extra Functions & Page Navigation
        if (btnKeyF1 != null) btnKeyF1.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f1), "[F1]", "[F1]"));
        if (btnKeyF9 != null) btnKeyF9.setOnClickListener(v -> sendKeyAction(getString(R.string.action_f9), "[F9]", "[F9]"));
        if (btnKeyPgUp != null) btnKeyPgUp.setOnClickListener(v -> sendKeyAction(getString(R.string.action_pgup), "[PGUP]", "[PGUP]"));
        if (btnKeyPgDn != null) btnKeyPgDn.setOnClickListener(v -> sendKeyAction(getString(R.string.action_pgdn), "[PGDN]", "[PGDN]"));

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

        // Terminal Actions (Clear, Copy, and Long-Click to Copy)
        btnClearLog.setOnClickListener(v -> tvSerialLog.setText(""));
        btnCopyLog.setOnClickListener(v -> copyLogToClipboard());

        View.OnLongClickListener copyListener = v -> {
            copyLogToClipboard();
            return true;
        };
        tvSerialLog.setOnLongClickListener(copyListener);
        scrollSerialLog.setOnLongClickListener(copyListener);

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
            int count = event.getPointerCount();
            long now = System.currentTimeMillis();

            switch (action) {
                case MotionEvent.ACTION_DOWN:
                    lastTouchX = event.getX();
                    lastTouchY = event.getY();
                    touchDownTime = now;
                    isDragging = false;
                    maxPointerCount = 1;
                    return true;

                case MotionEvent.ACTION_POINTER_DOWN:
                    maxPointerCount = Math.max(maxPointerCount, count);
                    return true;

                case MotionEvent.ACTION_MOVE:
                    float curX = event.getX();
                    float curY = event.getY();
                    float deltaX = curX - lastTouchX;
                    float deltaY = curY - lastTouchY;

                    // If 2 fingers present: handle vertical scrolling with mouse wheel
                    if (count >= 2) {
                        isDragging = true;
                        if (now - lastMoveSentTime >= MOUSE_THROTTLE_MS) {
                            if (Math.abs(deltaY) > 6f) {
                                int scrollDz = deltaY > 0 ? -1 : 1;
                                usbManager.sendMouseWheel(scrollDz);
                                lastMoveSentTime = now;
                                updateLastAction(getString(scrollDz > 0 ? R.string.action_wheel_scroll_up : R.string.action_wheel_scroll_down));
                            }
                            lastTouchX = curX;
                            lastTouchY = curY;
                        }
                        return true;
                    }

                    // 1 finger: relative cursor movement
                    if (Math.abs(deltaX) > 1.2f || Math.abs(deltaY) > 1.2f) {
                        isDragging = true;
                        if (now - lastMoveSentTime >= MOUSE_THROTTLE_MS) {
                            int dx = (int) (deltaX * 1.8f);
                            int dy = (int) (deltaY * 1.8f);
                            dx = Math.max(-127, Math.min(127, dx));
                            dy = Math.max(-127, Math.min(127, dy));
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
                        triggerHaptic();
                        if (maxPointerCount >= 2) {
                            usbManager.sendMouseClick(UsbSerialManager.MOUSE_BUTTON_RIGHT);
                            updateLastAction(getString(R.string.action_tap_2_fingers));
                        } else {
                            usbManager.sendMouseClick(UsbSerialManager.MOUSE_BUTTON_LEFT);
                            updateLastAction(getString(R.string.action_tap_1_finger));
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
        findViewById(R.id.key_bs).setOnClickListener(v -> sendKeyAction(getString(R.string.action_bs), "\b", "[BS]"));
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
            btnToggleMode.setBackgroundResource(R.drawable.bg_key_action);
            btnToggleMode.setTextColor(Color.WHITE);
            usbManager.sendString("#BIOS\n");
            logTerminalTx(getString(R.string.log_mode_bios_enabled));
        } else {
            btnToggleMode.setText(R.string.mode_text);
            btnToggleMode.setBackgroundResource(R.drawable.bg_key_bios_alert);
            btnToggleMode.setTextColor(Color.WHITE);
            usbManager.sendString("#TEXTO\n");
            logTerminalTx(getString(R.string.log_mode_text_enabled));
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

    private void copyLogToClipboard() {
        triggerHaptic();
        String text = tvSerialLog.getText().toString();
        if (TextUtils.isEmpty(text)) {
            Toast.makeText(this, R.string.copy_log_empty, Toast.LENGTH_SHORT).show();
            return;
        }
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            ClipData clip = ClipData.newPlainText(getString(R.string.clipboard_label), text);
            clipboard.setPrimaryClip(clip);
            Toast.makeText(this, R.string.copy_log_success, Toast.LENGTH_SHORT).show();
            updateLastAction(getString(R.string.action_copy_console));
        }
    }

    // --- USB SERIAL CALLBACKS ---

    @Override
    public void onUsbConnected(String deviceName) {
        tvUsbStatus.setText(getString(R.string.usb_connected_device, deviceName));
        tvUsbStatus.setTextColor(ContextCompat.getColor(this, R.color.status_green));
        ivUsbIcon.setColorFilter(ContextCompat.getColor(this, R.color.status_green));
        appendLog(getString(R.string.log_usb_connected_msg, deviceName));
        Toast.makeText(this, getString(R.string.arduino_connected_toast, deviceName), Toast.LENGTH_SHORT).show();
    }

    @Override
    public void onUsbDisconnected() {
        tvUsbStatus.setText(R.string.usb_disconnected);
        tvUsbStatus.setTextColor(ContextCompat.getColor(this, R.color.status_red));
        ivUsbIcon.setColorFilter(ContextCompat.getColor(this, R.color.text_muted));

        onPcLinkChanged(false, getString(R.string.pc_link_disconnected));
        appendLog(getString(R.string.log_usb_disconnected_msg));
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
                Toast.makeText(this, R.string.storage_permission_required, Toast.LENGTH_LONG).show();
            }
        }
    }

    private void executeFirmwareExport() {
        Toast.makeText(this, R.string.firmware_exporting, Toast.LENGTH_SHORT).show();
        appendLog(getString(R.string.log_firmware_extracting));

        FirmwareExporter.exportFirmwareAsync(this, new FirmwareExporter.ExportCallback() {
            @Override
            public void onSuccess(String targetDirectory, List<String> exportedFiles) {
                runOnUiThread(() -> {
                    String msg = getString(R.string.firmware_exported_success, targetDirectory, exportedFiles.size());
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
                    String err = getString(R.string.firmware_export_error, errorMessage);
                    Toast.makeText(MainActivity.this, err, Toast.LENGTH_LONG).show();
                    appendLog("[FIRMWARE ERROR] " + err);
                });
            }
        });
    }
}
