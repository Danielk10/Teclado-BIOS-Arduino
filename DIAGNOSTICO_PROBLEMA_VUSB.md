# Diagnóstico Técnico y Solución: Arduino UNO USB HID (V-USB) para BIOS / UEFI

Este documento detalla la causa raíz del fallo de conexión, el bucle de reinicios al encender la laptop y el problema del cierre inmediato de menús emergentes en la BIOS.

---

## 1. El Problema del Bucle de Reinicios al Encender la Laptop

### Síntoma
Cuando la laptop estaba apagada, el Arduino se comunicaba perfectamente con el teléfono vía Serial (CH340). Sin embargo, **en el instante exacto en que se encendía la laptop**, el Arduino comenzaba a reiniciarse cada 1.4 segundos en un bucle infinito, mostrando texto cortado en la terminal (`-> Envia 'w' pa^@^@^@...`).

### Causa Raíz: Incompatibilidad de Vector de Interrupción en AVR-GCC 7+
El microcontrolador ATmega328P no tiene hardware USB nativo en sus pines D2/D4; utiliza **V-USB**, una implementación por software en código ensamblador (`usbdrvasm.S`).

1. **Protocolo USB:** Cada vez que el controlador USB de la laptop sondea el bus o envía paquetes, se produce una transición lógica en el pin **D2 (D+)**, el cual está físicamente conectado a la interrupción externa **INT0** del microcontrolador.
2. **El bug de nombres en GCC:**
   * En versiones antiguas de AVR-GCC (año 2007), la interrupción `INT0` en ensamblador se nombraba como `SIG_INTERRUPT0`.
   * En compiladores modernos (GCC 7.3+ utilizado por ArduinoDroid y Arduino CLI), el alias `SIG_INTERRUPT0` fue descontinuado del mapeo del enlazador, y el vector oficial pasó a llamarse **`__vector_1`**.
3. **El salto a `__bad_interrupt`:**
   Al no coincidir el símbolo, el compilador ensambló la rutina de V-USB con un nombre no reconocido y dejó el vector 1 de la tabla de interrupciones apuntando a la función por defecto `__bad_interrupt`.
   
   Al desensamblar el binario compilado (`.elf`), se encontró la causa exacta:
   ```asm
   00000000 <__vectors>:
          0:   0c 94 e2 03    jmp  0x7c4   ; Vector 0: RESET
          4:   0c 94 0a 04    jmp  0x814   ; Vector 1 (INT0): Salta a __bad_interrupt!
          ...
   00000814 <__bad_interrupt>:
        814:   0c 94 00 00    jmp  0       ; jmp 0 = ¡REINICIAR EL MICROCONTROLADOR!
   ```
4. **La consecuencia:** Cada vez que la laptop encendía y enviaba su primer pulso eléctrico USB, el microcontrolador ejecutaba `jmp 0` (botón de reinicio por hardware). Como la laptop seguía enviando pulsos, el microcontrolador entraba en un bucle de reinicio eterno cada 1.4 segundos.

### Solución Aplicada
En los archivos `usbdrvasm.S` y `usbconfig.h`, se definió explícitamente:
```c
#define USB_INTR_VECTOR __vector_1
```
Con esto, la tabla de vectores ahora salta directamente al manejador USB nativo:
```asm
   4:   0c 94 8c 09    jmp  0x1318  <__vector_1>  ; ¡V-USB atiende la señal sin reiniciar!
```

---

## 2. El Problema del Cierre Inmediato del Menú / Spinner en la BIOS

### Síntoma
Al seleccionar una opción con submenú o lista desplegable en la BIOS (como el selector de idioma / Language), al enviar `ENTER`, la lista se desplegaba e inmediatamente se cerraba o parpadeaba en una fracción de segundo.

### Causa Raíz: Doble Pulsación Consecutiva (`repeats = 2`)
1. **Ráfagas en el código:** En las versiones iniciales se configuró una ráfaga de 2 repeticiones (`repeats = 2`) para garantizar que la BIOS capturara las teclas durante el arranque lento del POST.
2. **Comportamiento en la BIOS:**
   * En el bucle de eventos normal de la BIOS (Setup GUI), un menú desplegable funciona así:
     * Pulsación 1 de ENTER $\to$ Despliega la lista de opciones.
     * Pulsación 2 de ENTER $\to$ Confirma la opción actualmente seleccionada y cierra la lista.
   * Al enviar el comando con `repeats = 2`, el Arduino enviaba dos pulsaciones de Enter separadas por apenas 20 ms. La BIOS interpretaba: *Abrir lista $\to$ Confirmar y cerrar lista* en menos de 40 ms.
3. **Retorno de carro (`\r\n`):** Muchas aplicaciones de terminal serie envían tanto retorno de carro (`\r`) como salto de línea (`\n`), lo que causaba un segundo envío inadvertido si no se filtraba.

### Solución Aplicada
1. Se redujo el número de repeticiones a **`1`** en todas las teclas de navegación y confirmación (`ENTER`, `ESC`, `ARRIBA`, `ABAJO`, `IZQUIERDA`, `DERECHA`, `ESPACIO`, `F10`).
2. Se añadió un tiempo de retención de tecla activa de **25 ms** en `sendKeyStroke()` antes de enviar el reporte de liberación (Key-Up), simulando una pulsación humana limpia.
3. Se implementó un filtro antirrebote de 80 ms para secuencias `\r\n` en la recepción serie.

---

## 3. Requisitos Eléctricos Críticos del Bus V-USB (Hardware a 5V)

Para que la PC detecte el teclado sin un sistema operativo presente (en frío):

1. **Resistencia Pull-up obligatoria de 1.5 kΩ en D- (Cable Blanco) hacia 5V:**
   * La resistencia pull-up de 1.5 kΩ se conecta directamente al **Pin 5V del Arduino** (o al pin **D5** configurado por firmware en estado ALTO a 5V), **NO a la salida de 3.3V**.
   * El diodo Zener de 3.6V conectado entre la línea de datos y GND se encarga de limitar la tensión al rango seguro de ~3.6V que exige el estándar USB Low-Speed, protegiendo el chipset de la PC y garantizando la detección por hardware.
   * Sin esta resistencia conectada a 5V/D5, la PC lee 0V constante y asume que el puerto está completamente vacío.
2. **Resistencias de 68 Ω en serie:**
   * Pin D2 (Verde / D+) y Pin D4 (Blanco / D-) deben llevar una resistencia de 68 Ω en serie para adaptación de impedancia y protección.
3. **Masa Común (GND):**
   * El cable negro del USB de la PC debe estar conectado obligatoriamente al pin GND del Arduino.
4. **Aislamiento del Cable Rojo (+5V de la PC):**
   * El Arduino ya se alimenta desde el teléfono móvil mediante USB-OTG.
   * **El cable rojo de 5V de la PC NUNCA debe conectarse al Arduino**, ya que chocaría con la alimentación del teléfono provocando caídas de tensión y cortes de corriente por protección del teléfono.

---

## 4. Firmware Versión 2.0: Ratón Compuesto, Antirrebote Hardware y Protocolo

En la versión 2.0 se incorporan mejoras esenciales para aplicaciones de diagnóstico e interacción gráfica (UEFI Setup con cursor de ratón):

1. **Descriptor HID Compuesto con Report IDs:**
   * En USB Low-Speed (1.5 Mbps), los paquetes de interrupción no pueden superar 8 bytes.
   * Se configuró un descriptor HID unificado con dos colecciones lógicas:
     * **Report ID 1 (Teclado):** 8 bytes (1 byte ID, 1 byte modificadores, 6 bytes códigos de tecla).
     * **Report ID 2 (Ratón):** 5 bytes (1 byte ID, 1 byte botones [bit0=Izq, bit1=Der, bit2=Centro], 1 byte delta X, 1 byte delta Y, 1 byte rueda delta Z).
   * Tanto Windows, Linux, Android como BIOS Setup UEFI reconocen el Arduino simultáneamente como teclado y ratón de 3 botones con rueda.

2. **Temporizador de Milisegundos sin Interrupciones (Timer1):**
   * Para evitar cualquier conflicto de jitter con V-USB al desactivar la interrupción Timer0 (`TIMSK0 &= ~(1 << TOIE0)`), se configuró **Timer1** a 62.5 kHz (prescaler 256) en modo libre sin interrupciones.
   * Esto proporciona un contador de milisegundos 100% exacto para el filtro antirrebote de 80 ms sin añadir ciclos de interrupción a la CPU.

3. **Protocolo de Sincronización Machine-Readable para Android:**
   * `#PING` / `#SYNC` -> `#PONG:BIOS_KEYBOARD_V2:LINK=[0|1],PKTS=Z`
   * `#STATUS` -> `#STAT:LINK=1,ADDR=X,CFG=Y,PKTS=Z` o `#STAT:LINK=0`

