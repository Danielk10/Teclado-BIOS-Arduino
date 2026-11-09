# UsbKeyboardBIOS — Librería USB HID compuesta (V-USB)

Librería que usa `arduino_bios_keyboard_v2.ino` para exponer **un único
dispositivo USB** con dos interfaces HID: teclado de arranque (BIOS) y ratón.

Se reconstruyó a partir de los descriptores USB embebidos en el firmware
original, de modo que la identidad USB es **idéntica** a la de la versión 2.0:

| Elemento            | Valor                                              |
| ------------------- | -------------------------------------------------- |
| VID:PID             | `16c0:27dc`                                        |
| bcdDevice           | `0x0100`                                           |
| Fabricante (str 1)  | `Arduino`                                          |
| Producto (str 2)    | `BIOS Kbd Mouse`                                   |
| Interfaz 0          | HID boot, protocolo teclado, EP1 IN, 8 B, 10 ms    |
| Interfaz 1          | HID boot, protocolo ratón, EP3 IN, 8 B, 10 ms      |
| Report teclado      | 63 bytes                                           |
| Report ratón        | 62 bytes                                           |
| Cableado            | D+ = PD2 (INT0), D- = PD4                          |

## Archivos

- `UsbKeyboard.h` — API pública (`UsbKeyboard`), keycodes y modificadores.
- `UsbKeyboard.cpp` — instancia global.
- `UsbKeyboardDescriptors.cpp` — descriptores USB y `usbFunctionDescriptor()`.
- `usbconfig.h` — configuración de V-USB (pines, endpoints, VID/PID).
- `usbdrv.c`, `usbdrv.h`, `usbdrvasm.S`, `usbdrvasm*.inc`, `asmcommon.inc`,
  `usbportability.h`, `oddebug.*` — núcleo V-USB 20121206.

## Compilar

```bash
./firmware/build.sh
```

Requiere `arduino-cli` y el core `arduino:avr`:

```bash
arduino-cli core install arduino:avr
```

## API

```cpp
UsbKeyboard.init();
UsbKeyboard.update();

UsbKeyboard.sendKeyStroke(KEY_ENTER);                 // -> bool
UsbKeyboard.sendKeyStroke(KEY_DELETE, MOD_CONTROL_LEFT | MOD_ALT_LEFT);
UsbKeyboard.isConnectedToHost();                      // -> bool
UsbKeyboard.getUsbAddress();
UsbKeyboard.getUsbConfiguration();
UsbKeyboard.getPacketCount();

UsbKeyboard.mouseMove(dx, dy);
UsbKeyboard.mouseClick(1);        // 1 = izquierdo, 2 = derecho, 4 = central
UsbKeyboard.mousePress(1);
UsbKeyboard.mouseRelease(1);      // 0xFF libera todos
UsbKeyboard.mouseScroll(dz);
```

## Licencia

El núcleo `usbdrv.*` es **V-USB** de Objective Development y se distribuye bajo
**GNU GPL v2** (o licencia comercial). Ver `LICENSE-VUSB.txt`.

Ese requisito aplica a los archivos de esta carpeta y al firmware que los
enlaza; el resto del repositorio (aplicación Android) sigue bajo Apache 2.0.
