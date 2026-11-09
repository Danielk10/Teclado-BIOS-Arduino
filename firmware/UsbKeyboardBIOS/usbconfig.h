/* usbconfig.h
 * Configuración V-USB para Teclado BIOS Arduino (compuesto Teclado + Ratón).
 *
 * Reconstruida a partir de los descriptores USB embebidos en el firmware
 * arduino_bios_keyboard_v2.hex original, de modo que la enumeración USB
 * resultante es idéntica a la del binario de referencia:
 *
 *   VID:PID      16c0:27dc
 *   bcdDevice    0x0100
 *   Fabricante   "Arduino"          (string 1)
 *   Producto     "BIOS Kbd Mouse"   (string 2)
 *   Interfaz 0   HID, boot, protocolo teclado, EP1 IN (8 bytes, 10 ms)
 *   Interfaz 1   HID, boot, protocolo ratón,   EP3 IN (8 bytes, 10 ms)
 *
 * Cableado: D+ = PD2 (INT0), D- = PD4.
 */

#ifndef __usbconfig_h_included__
#define __usbconfig_h_included__

/* ---------------------------- Hardware Config ---------------------------- */

#define USB_CFG_IOPORTNAME      D
#define USB_CFG_DMINUS_BIT      4
#define USB_CFG_DPLUS_BIT       2
#define USB_CFG_CLOCK_KHZ       (F_CPU/1000)

#define USB_CFG_CHECK_CRC       0
#define USB_CFG_CHECK_DATA_TOGGLING 0

/* --------------------------- Functional Range ---------------------------- */

#define USB_CFG_HAVE_INTRIN_ENDPOINT    1
#define USB_CFG_HAVE_INTRIN_ENDPOINT3   1
#define USB_CFG_EP3_NUMBER              3

#define USB_CFG_IS_SELF_POWERED         0
#define USB_CFG_MAX_BUS_POWER           100

#define USB_CFG_IMPLEMENT_FN_WRITE      0
#define USB_CFG_IMPLEMENT_FN_READ       0
#define USB_CFG_IMPLEMENT_FN_WRITEOUT   0
#define USB_CFG_HAVE_MEASURE_FRAME_LENGTH 0

/* -------------------------- Device Description --------------------------- */

#define USB_CFG_VENDOR_ID       0xc0, 0x16
#define USB_CFG_DEVICE_ID       0xdc, 0x27
#define USB_CFG_DEVICE_VERSION  0x00, 0x01

#define USB_CFG_VENDOR_NAME     'A', 'r', 'd', 'u', 'i', 'n', 'o'
#define USB_CFG_VENDOR_NAME_LEN 7
#define USB_CFG_DEVICE_NAME     'B', 'I', 'O', 'S', ' ', 'K', 'b', 'd', ' ', 'M', 'o', 'u', 's', 'e'
#define USB_CFG_DEVICE_NAME_LEN 14

#define USB_CFG_DEVICE_CLASS        0
#define USB_CFG_DEVICE_SUBCLASS     0

#define USB_CFG_INTERFACE_CLASS     3
#define USB_CFG_INTERFACE_SUBCLASS  1
#define USB_CFG_INTERFACE_PROTOCOL  1

/* ------------------- Fine Control over USB Descriptors ------------------- */

/* (1 << 14) == USB_PROP_IS_DYNAMIC: usbdrv.h pide los descriptores a
 * usbFunctionDescriptor(), implementado en UsbKeyboard.h. */
#define USB_CFG_DESCR_PROPS_DEVICE                  (1 << 14)
#define USB_CFG_DESCR_PROPS_CONFIGURATION           (1 << 14)
#define USB_CFG_DESCR_PROPS_STRINGS                 0
#define USB_CFG_DESCR_PROPS_STRING_0                0
#define USB_CFG_DESCR_PROPS_STRING_VENDOR           0
#define USB_CFG_DESCR_PROPS_STRING_PRODUCT          0
#define USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER    0
#define USB_CFG_DESCR_PROPS_HID                     (1 << 14)
#define USB_CFG_DESCR_PROPS_HID_REPORT              (1 << 14)
#define USB_CFG_DESCR_PROPS_UNKNOWN                 0

#endif /* __usbconfig_h_included__ */
