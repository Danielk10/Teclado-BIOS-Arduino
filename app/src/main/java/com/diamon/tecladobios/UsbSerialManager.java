package com.diamon.tecladobios;

import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import com.hoho.android.usbserial.util.SerialInputOutputManager;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.concurrent.Executors;

/**
 * Manages USB Serial communication with Arduino Uno (CH340 / CP2102 / FTDI / CDC ACM).
 * Runs at 9600 baud, 8N1. Parses PC-link status responses and handles keyboard and mouse events.
 */
public class UsbSerialManager implements SerialInputOutputManager.Listener {

    private static final String TAG = "UsbSerialManager";
    public static final String ACTION_USB_PERMISSION = "com.diamon.tecladobios.USB_PERMISSION";
    private static final int BAUD_RATE = 9600;

    // Mouse button constants
    public static final int MOUSE_BUTTON_LEFT = 1;
    public static final int MOUSE_BUTTON_RIGHT = 2;
    public static final int MOUSE_BUTTON_MIDDLE = 4;

    public interface UsbListener {
        void onUsbConnected(String deviceName);
        void onUsbDisconnected();
        void onPcLinkChanged(boolean isLinked, String details);
        void onDataReceived(String data);
        void onError(String message);
    }

    private final Context context;
    private final UsbListener listener;
    private final UsbManager usbManager;
    private final Handler mainHandler;

    private UsbSerialPort serialPort;
    private SerialInputOutputManager ioManager;
    private boolean isConnected = false;
    private boolean isPcLinked = false;
    private boolean receiverRegistered = false;

    // Periodic ping timer
    private final Runnable pingRunnable = new Runnable() {
        @Override
        public void run() {
            if (isConnected) {
                // Send lightweight ping / status query to Arduino
                sendString("?");
                mainHandler.postDelayed(this, 3500);
            }
        }
    };

    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context ctx, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if (device != null) {
                            Log.d(TAG, "USB Permission granted for: " + device.getDeviceName());
                            openDevice(device);
                        }
                    } else {
                        Log.w(TAG, "USB Permission denied");
                        listener.onError("Permiso USB denegado por el usuario.");
                    }
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                Log.d(TAG, "USB Device Attached broadcast received");
                connect();
            } else if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                Log.d(TAG, "USB Device Detached broadcast received");
                disconnect();
            }
        }
    };

    public UsbSerialManager(Context context, UsbListener listener) {
        this.context = context;
        this.listener = listener;
        this.usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        this.mainHandler = new Handler(Looper.getMainLooper());
    }

    public void register() {
        if (!receiverRegistered) {
            IntentFilter filter = new IntentFilter();
            filter.addAction(ACTION_USB_PERMISSION);
            filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
            filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                context.registerReceiver(usbReceiver, filter, Context.RECEIVER_EXPORTED);
            } else {
                context.registerReceiver(usbReceiver, filter);
            }
            receiverRegistered = true;
        }
    }

    public void unregister() {
        if (receiverRegistered) {
            try {
                context.unregisterReceiver(usbReceiver);
            } catch (Exception ignored) {}
            receiverRegistered = false;
        }
        disconnect();
    }

    public void connect() {
        if (isConnected) return;

        List<UsbSerialDriver> availableDrivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);
        if (availableDrivers.isEmpty()) {
            listener.onError("No se detectó ningún dispositivo USB serial.");
            return;
        }

        UsbSerialDriver driver = availableDrivers.get(0);
        UsbDevice device = driver.getDevice();

        if (!usbManager.hasPermission(device)) {
            int flags = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M ? PendingIntent.FLAG_IMMUTABLE : 0;
            PendingIntent permissionIntent = PendingIntent.getBroadcast(
                    context, 0, new Intent(ACTION_USB_PERMISSION), flags);
            usbManager.requestPermission(device, permissionIntent);
        } else {
            openDevice(device);
        }
    }

    private void openDevice(UsbDevice device) {
        List<UsbSerialDriver> availableDrivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager);
        UsbSerialDriver driver = null;
        for (UsbSerialDriver d : availableDrivers) {
            if (d.getDevice().equals(device)) {
                driver = d;
                break;
            }
        }
        if (driver == null && !availableDrivers.isEmpty()) {
            driver = availableDrivers.get(0);
        }
        if (driver == null) {
            listener.onError("No se pudo obtener el controlador para el dispositivo USB.");
            return;
        }

        UsbDeviceConnection connection = usbManager.openDevice(driver.getDevice());
        if (connection == null) {
            listener.onError("Fallo al abrir la conexión USB (revisa permisos OTG).");
            return;
        }

        try {
            serialPort = driver.getPorts().get(0);
            serialPort.open(connection);
            serialPort.setParameters(BAUD_RATE, UsbSerialPort.DATABITS_8,
                    UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);

            // Set DTR / RTS to allow standard Arduino serial handshaking
            serialPort.setDTR(true);
            serialPort.setRTS(true);

            ioManager = new SerialInputOutputManager(serialPort, this);
            Executors.newSingleThreadExecutor().submit(ioManager);

            isConnected = true;
            String devName = driver.getClass().getSimpleName().replace("SerialDriver", "");
            if (devName.isEmpty()) devName = "Arduino USB";

            final String finalName = devName;
            mainHandler.post(() -> listener.onUsbConnected(finalName));

            // Start periodic link check query
            mainHandler.postDelayed(pingRunnable, 1000);

        } catch (IOException e) {
            Log.e(TAG, "Error opening serial port", e);
            disconnect();
            listener.onError("Error al abrir puerto serial: " + e.getMessage());
        }
    }

    public void disconnect() {
        mainHandler.removeCallbacks(pingRunnable);
        if (ioManager != null) {
            ioManager.setListener(null);
            ioManager.stop();
            ioManager = null;
        }
        if (serialPort != null) {
            try {
                serialPort.close();
            } catch (IOException ignored) {}
            serialPort = null;
        }
        if (isConnected) {
            isConnected = false;
            isPcLinked = false;
            mainHandler.post(listener::onUsbDisconnected);
        }
    }

    public boolean isConnected() {
        return isConnected;
    }

    public boolean isPcLinked() {
        return isPcLinked;
    }

    public boolean sendString(String data) {
        if (!isConnected || serialPort == null) return false;
        try {
            byte[] bytes = data.getBytes(StandardCharsets.US_ASCII);
            serialPort.write(bytes, 1000);
            return true;
        } catch (IOException e) {
            Log.e(TAG, "Error writing data to serial", e);
            return false;
        }
    }

    public boolean sendChar(char c) {
        return sendString(String.valueOf(c));
    }

    public boolean sendKeyTag(String tag) {
        // Sends e.g. "[DEL]", "[ENTER]", etc.
        return sendString(tag);
    }

    // --- MOUSE COMMANDS (Top Touchpad & Buttons) ---

    /**
     * Send relative mouse movement dx, dy: "#M:dx,dy\n"
     */
    public boolean sendMouseMove(int dx, int dy) {
        return sendString("#M:" + dx + "," + dy + "\n");
    }

    /**
     * Send instant mouse click: Left=#ML, Right=#MR, Middle=#MM
     */
    public boolean sendMouseClick(int button) {
        switch (button) {
            case MOUSE_BUTTON_LEFT:
                return sendString("#ML\n");
            case MOUSE_BUTTON_MIDDLE:
                return sendString("#MM\n");
            case MOUSE_BUTTON_RIGHT:
                return sendString("#MR\n");
            default:
                return false;
        }
    }

    /**
     * Send mouse button press (for drag/selection): "#MD:L\n", "#MD:R\n", "#MD:M\n"
     */
    public boolean sendMouseDown(int button) {
        if (button == MOUSE_BUTTON_RIGHT) return sendString("#MD:R\n");
        if (button == MOUSE_BUTTON_MIDDLE) return sendString("#MD:M\n");
        return sendString("#MD:L\n");
    }

    /**
     * Send mouse button release: "#MU:L\n", "#MU:R\n", "#MU:M\n"
     */
    public boolean sendMouseUp(int button) {
        if (button == MOUSE_BUTTON_RIGHT) return sendString("#MU:R\n");
        if (button == MOUSE_BUTTON_MIDDLE) return sendString("#MU:M\n");
        if (button == 0xFF) return sendString("#MU\n");
        return sendString("#MU:L\n");
    }

    /**
     * Send mouse wheel scroll: "#MW:dz\n" (positive = up, negative = down)
     */
    public boolean sendMouseWheel(int dz) {
        return sendString("#MW:" + dz + "\n");
    }

    // --- PROTOCOL PARSING (Arduino V1 and V2) ---

    @Override
    public void onNewData(byte[] data) {
        if (data == null || data.length == 0) return;
        String text = new String(data, StandardCharsets.UTF_8);

        mainHandler.post(() -> {
            listener.onDataReceived(text);
            parsePcLinkStatus(text);
        });
    }

    @Override
    public void onRunError(Exception e) {
        Log.e(TAG, "Serial IO Manager error", e);
        mainHandler.post(() -> {
            disconnect();
            listener.onError("Conexión serial perdida: " + e.getMessage());
        });
    }

    private void parsePcLinkStatus(String raw) {
        if (raw == null) return;
        String line = raw.toUpperCase();

        // Check for positive PC link signals
        if (line.contains("¡LA PC HA RECONOCIDO EL TECLADO!")
                || line.contains("PC COMUNICADA: SI")
                || line.contains("#PONG:BIOS_KEYBOARD_V2:LINK=1")
                || line.contains("#STAT:LINK=1")
                || line.contains("LINK=1")) {
            if (!isPcLinked) {
                isPcLinked = true;
                listener.onPcLinkChanged(true, "PC Conectada (HID Activo)");
            }
        }
        // Check for disconnected PC signals
        else if (line.contains("LA PC SE HA DESCONECTADO")
                || line.contains("PC COMUNICADA: NO")
                || line.contains("#PONG:BIOS_KEYBOARD_V2:LINK=0")
                || line.contains("#STAT:LINK=0")
                || line.contains("LINK=0")) {
            if (isPcLinked) {
                isPcLinked = false;
                listener.onPcLinkChanged(false, "PC Desconectada");
            }
        }
    }
}
