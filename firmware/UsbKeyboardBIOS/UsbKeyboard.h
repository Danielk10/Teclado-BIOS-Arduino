/*
 * UsbKeyboard.h - Librería USB HID compuesta Teclado + Ratón (V-USB)
 *
 * Reconstruida para el firmware "Teclado BIOS Arduino v2".
 * Mantiene exactamente la misma identidad USB y los mismos report
 * descriptors que el firmware de referencia (16c0:27dc, 2 interfaces HID),
 * de modo que la BIOS / UEFI la detecta igual que antes.
 *
 * API usada por el sketch:
 *   UsbKeyboard.init()
 *   UsbKeyboard.update()
 *   UsbKeyboard.sendKeyStroke(keycode, modifiers)  -> bool
 *   UsbKeyboard.isConnectedToHost()                -> bool
 *   UsbKeyboard.getUsbAddress()                    -> byte
 *   UsbKeyboard.getUsbConfiguration()              -> byte
 *   UsbKeyboard.getPacketCount()                   -> unsigned long
 *   UsbKeyboard.mouseMove(dx, dy)
 *   UsbKeyboard.mouseClick(btn)   btn: 1=izq, 2=der, 4=central
 *   UsbKeyboard.mousePress(btn)
 *   UsbKeyboard.mouseRelease(btn)  (0xFF libera todos)
 *   UsbKeyboard.mouseScroll(dz)
 */

#ifndef USBKEYBOARD_H
#define USBKEYBOARD_H

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>

extern "C" {
#include "usbdrv.h"
}

extern uchar usbDeviceAddr;

/* ----------------------------- Keycodes HID ------------------------------ */

#define KEY_A           4
#define KEY_B           5
#define KEY_C           6
#define KEY_D           7
#define KEY_E           8
#define KEY_F           9
#define KEY_G           10
#define KEY_H           11
#define KEY_I           12
#define KEY_J           13
#define KEY_K           14
#define KEY_L           15
#define KEY_M           16
#define KEY_N           17
#define KEY_O           18
#define KEY_P           19
#define KEY_Q           20
#define KEY_R           21
#define KEY_S           22
#define KEY_T           23
#define KEY_U           24
#define KEY_V           25
#define KEY_W           26
#define KEY_X           27
#define KEY_Y           28
#define KEY_Z           29

#define KEY_1           30
#define KEY_2           31
#define KEY_3           32
#define KEY_4           33
#define KEY_5           34
#define KEY_6           35
#define KEY_7           36
#define KEY_8           37
#define KEY_9           38
#define KEY_0           39

#define KEY_ENTER       40
#define KEY_ESC         41
#define KEY_BACKSPACE   42
#define KEY_TAB         43
#define KEY_SPACE       44

#define KEY_MINUS       45
#define KEY_EQUAL       46
#define KEY_LEFTBRACE   47
#define KEY_RIGHTBRACE  48
#define KEY_BACKSLASH   49
#define KEY_SEMICOLON   51
#define KEY_APOSTROPHE  52
#define KEY_GRAVE       53
#define KEY_COMMA       54
#define KEY_DOT         55
#define KEY_SLASH       56
#define KEY_CAPSLOCK    57

#define KEY_F1          58
#define KEY_F2          59
#define KEY_F3          60
#define KEY_F4          61
#define KEY_F5          62
#define KEY_F6          63
#define KEY_F7          64
#define KEY_F8          65
#define KEY_F9          66
#define KEY_F10         67
#define KEY_F11         68
#define KEY_F12         69

#define KEY_PRINTSCREEN 70
#define KEY_SCROLLLOCK  71
#define KEY_PAUSE       72
#define KEY_INSERT      73
#define KEY_HOME        74
#define KEY_PAGEUP      75
#define KEY_DELETE      76
#define KEY_END         77
#define KEY_PAGEDOWN    78
#define KEY_RIGHT_ARROW 79
#define KEY_LEFT_ARROW  80
#define KEY_DOWN_ARROW  81
#define KEY_UP_ARROW    82

/* Alias usados por versiones anteriores */
#define KEY_ARROW_LEFT   KEY_LEFT_ARROW
#define KEY_ARROW_RIGHT  KEY_RIGHT_ARROW
#define KEY_ARROW_UP     KEY_UP_ARROW
#define KEY_ARROW_DOWN   KEY_DOWN_ARROW
#define KEY_PGUP         KEY_PAGEUP
#define KEY_PGDN         KEY_PAGEDOWN
#define KEY_DEL          KEY_DELETE

/* --------------------------- Modificadores HID ---------------------------- */

#define MOD_CONTROL_LEFT    0x01
#define MOD_SHIFT_LEFT      0x02
#define MOD_ALT_LEFT        0x04
#define MOD_GUI_LEFT        0x08
#define MOD_CONTROL_RIGHT   0x10
#define MOD_SHIFT_RIGHT     0x20
#define MOD_ALT_RIGHT       0x40
#define MOD_GUI_RIGHT       0x80

/* ------------------------------- Clase ----------------------------------- */

class UsbKeyboardDevice {
public:
    UsbKeyboardDevice() : mouseButtons(0), packetCount(0) {}

    /* Reinicia el bus USB (SE0) y arranca el driver. */
    void init() {
        usbDeviceDisconnect();
        _delay_ms(250);
        usbDeviceConnect();
        mouseButtons = 0;
        usbInit();
        sei();
    }

    /* Debe llamarse con frecuencia (al menos cada 50 ms). */
    void update() {
        usbPoll();
    }

    /* Envía una pulsación completa (press + release).
     * Devuelve true si el host aceptó el reporte de pulsación. */
    bool sendKeyStroke(byte keycode, byte modifiers = 0) {
        byte press[8]   = { modifiers, 0x00, keycode, 0x00, 0x00, 0x00, 0x00, 0x00 };
        byte release[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

        if (!waitKeyboardReady()) {
            return false;
        }
        usbSetInterrupt(press, sizeof(press));
        packetCount++;

        /* Esperar a que el press salga antes de encolar el release. */
        if (!waitKeyboardReady()) {
            return false;
        }
        usbSetInterrupt(release, sizeof(release));
        packetCount++;
        return true;
    }

    /* El host ya configuró el dispositivo (SET_CONFIGURATION). */
    bool isConnectedToHost() {
        return usbConfiguration != 0;
    }

    byte getUsbAddress() {
        return usbDeviceAddr;
    }

    byte getUsbConfiguration() {
        return usbConfiguration;
    }

    unsigned long getPacketCount() {
        return packetCount;
    }

    /* ---------------------------- Ratón ---------------------------- */

    void mouseMove(int8_t dx, int8_t dy) {
        sendMouseReport(dx, dy, 0);
    }

    void mouseScroll(int8_t dz) {
        sendMouseReport(0, 0, dz);
    }

    void mousePress(byte btn) {
        mouseButtons |= btn;
        sendMouseReport(0, 0, 0);
    }

    void mouseRelease(byte btn) {
        mouseButtons &= (byte)~btn;
        sendMouseReport(0, 0, 0);
    }

    void mouseClick(byte btn) {
        mousePress(btn);
        _delay_ms(2);
        mouseRelease(btn);
    }

private:
    byte mouseButtons;
    unsigned long packetCount;

    bool waitKeyboardReady() {
        for (uint16_t i = 0; i < 400; i++) {   /* ~20 ms */
            if (usbInterruptIsReady()) {
                return true;
            }
            usbPoll();
            _delay_us(50);
        }
        return false;
    }

    bool waitMouseReady() {
        for (uint16_t i = 0; i < 400; i++) {   /* ~20 ms */
            if (usbInterruptIsReady3()) {
                return true;
            }
            usbPoll();
            _delay_us(50);
        }
        return false;
    }

    void sendMouseReport(int8_t dx, int8_t dy, int8_t dz) {
        byte report[4] = { mouseButtons, (byte)dx, (byte)dy, (byte)dz };
        if (!waitMouseReady()) {
            return;
        }
        usbSetInterrupt3(report, sizeof(report));
        packetCount++;
    }
};

extern UsbKeyboardDevice UsbKeyboard;

#endif /* USBKEYBOARD_H */
