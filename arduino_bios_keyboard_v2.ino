/*
 * Arduino UNO USB HID Composite Keyboard + Mouse - VERSION 2.0
 * Compatible con BIOS / UEFI, Boot Menu, GRUB, Windows, Linux, Android y Shell.
 *
 * Características Version 2.0:
 * 1. Soporte Compuesto USB HID Teclado + Ratón (Report ID 1: Keyboard, Report ID 2: Mouse).
 * 2. Emulación completa de Ratón vía Serial:
 *    - #M:dx,dy  -> Movimiento relativo del ratón
 *    - #ML       -> Clic izquierdo (Left Click) o #MB:1
 *    - #MR       -> Clic derecho (Right Click) o #MB:2
 *    - #MM       -> Clic central (Middle Click) o #MB:4
 *    - #MD:btn   -> Botón presionado (arrastre / drag) (btn = 1|2|4 o L|R|M)
 *    - #MU:btn   -> Botón liberado (drop) (btn = 1|2|4 o L|R|M o vacío para todos)
 *    - #MW:dz    -> Rueda de desplazamiento (scroll)
 * 3. Protocolo de sincronización con App Android (OTG):
 *    - #PING / #SYNC -> Responde '#PONG:BIOS_KEYBOARD_V2:LINK=[0|1],PKTS=N'
 *    - #STATUS       -> Responde '#STAT:LINK=1,ADDR=X,CFG=Y,PKTS=Z' o '#STAT:LINK=0'
 * 4. Antirrebote de hardware/software (Debounce) en ENTER y teclas de navegación:
 *    - Evita que menús desplegables (spinners) de BIOS se abran y cierren accidentalmente.
 *    - Filtro de 80ms para secuencias \r\n y pulsaciones repetidas rápidas.
 *    - Temporizador por hardware Timer1 a 62.5 kHz sin interrupciones para mantener
 *      estabilidad absoluta en el bus USB bit-banging (V-USB).
 * 5. 100% retrocompatible con Version 1.0:
 *    - Modo BIOS rápido (s=Supr, u=Arriba, d=Abajo, e=Enter, x=Esc, 2=F2, 0=F10, etc.)
 *    - Modo Texto completo y etiquetas especiales entre corchetes [F1]-[F12], [UP], [CAD].
 *    - Comandos de diagnóstico #TEXTO, #BIOS, #AYUDA, #TEST, #RESET y '?'.
 */

#include "UsbKeyboard.h"

// Definición de códigos HID adicionales no incluidos en el header base
#define HID_KEY_BACKSPACE   42
#define HID_KEY_TAB         43
#define HID_KEY_MINUS       45  // - y _
#define HID_KEY_EQUAL       46  // = y +
#define HID_KEY_LEFTBRACE   47  // [ y {
#define HID_KEY_RIGHTBRACE  48  // ] y }
#define HID_KEY_BACKSLASH   49  // \ y |
#define HID_KEY_SEMICOLON   51  // ; y :
#define HID_KEY_APOSTROPHE  52  // ' y "
#define HID_KEY_GRAVE       53  // ` y ~
#define HID_KEY_COMMA       54  // , y <
#define HID_KEY_DOT         55  // . y >
#define HID_KEY_SLASH       56  // / y ?
#define HID_KEY_CAPSLOCK    57

// Modos de operación
enum KeyboardMode {
  MODE_FULL_TEXT = 1,  // Modo texto completo + etiquetas especiales [KEY]
  MODE_QUICK_BIOS = 2  // Modo de una sola tecla para acceso rápido a BIOS
};

KeyboardMode currentMode = MODE_QUICK_BIOS;

// ---------------------------------------------------------------------------
// Temporizador preciso sin interrupciones (Timer1 a prescaler 256 = 62.5 kHz)
// Mantiene exactitud de milisegundos sin generar interrupciones que interfieran
// con la sincronización crítica de V-USB.
// ---------------------------------------------------------------------------
static uint16_t lastTcnt1 = 0;
static unsigned long totalMs = 0;
static uint16_t tickRemainder = 0;

void initDebounceTimer() {
  TCCR1A = 0;
  TCCR1B = (1 << CS12); // Prescaler 256 -> 62500 ticks/seg (62.5 ticks/ms)
  TCNT1 = 0;
  lastTcnt1 = 0;
  totalMs = 0;
  tickRemainder = 0;
}

unsigned long getMillis() {
  uint16_t nowTcnt = TCNT1;
  uint16_t delta = nowTcnt - lastTcnt1;
  lastTcnt1 = nowTcnt;
  tickRemainder += delta;
  if (tickRemainder >= 125) {
    uint16_t ms2 = tickRemainder / 125;
    totalMs += (uint32_t)ms2 * 2;
    tickRemainder %= 125;
  }
  return totalMs + (tickRemainder >= 63 ? 1 : 0);
}

// ---------------------------------------------------------------------------
// Filtro antirrebote (Debounce) para ENTER y teclas de navegación
// ---------------------------------------------------------------------------
#define DEBOUNCE_INTERVAL_MS 80

bool isNavOrEnterKey(uint8_t keycode) {
  switch (keycode) {
    case KEY_ENTER:
    case KEY_UP_ARROW:
    case KEY_DOWN_ARROW:
    case KEY_LEFT_ARROW:
    case KEY_RIGHT_ARROW:
    case KEY_ESC:
    case KEY_F10:
    case HID_KEY_TAB:
    case KEY_HOME:
    case KEY_END:
    case KEY_PAGEUP:
    case KEY_PAGEDOWN:
    case KEY_SPACE:
    case HID_KEY_EQUAL:
    case HID_KEY_MINUS:
      return true;
    default:
      return false;
  }
}

bool shouldDebounce(uint8_t keycode) {
  if (!isNavOrEnterKey(keycode)) {
    return false; // Caracteres alfanuméricos no se filtran (permite 'ee', '00', etc.)
  }
  static unsigned long lastNavKeyTime = 0;
  static uint8_t lastNavKey = 0;
  unsigned long now = getMillis();

  if (keycode == lastNavKey && (now - lastNavKeyTime < DEBOUNCE_INTERVAL_MS)) {
    return true; // Rebote detectado -> descartar pulsación repetida
  }
  lastNavKey = keycode;
  lastNavKeyTime = now;
  return false;
}

// ---------------------------------------------------------------------------
// Ayuda e Información de Diagnóstico
// ---------------------------------------------------------------------------
void printHelp() {
  Serial.println(F("\n================ AYUDA TECLADO + RATON V2.0 ================"));
  Serial.println(F("MODO ACTUAL: "));
  Serial.println(currentMode == MODE_FULL_TEXT ? F("-> MODO TEXTO COMPLETO") : F("-> MODO BIOS RAPIDO"));
  
  Serial.println(F("\n[COMANDOS DE RATON (MOUSE HID)]"));
  Serial.println(F("  #M:dx,dy      : Mover raton relativo (ej. #M:20,-10)"));
  Serial.println(F("  #ML           : Clic izquierdo (Left Click)"));
  Serial.println(F("  #MR           : Clic derecho (Right Click)"));
  Serial.println(F("  #MM           : Clic central (Middle Click)"));
  Serial.println(F("  #MB:n         : Clic en boton n (1=Izq, 2=Der, 4=Medio)"));
  Serial.println(F("  #MD:btn       : Pulsar y mantener boton (Arrastrar / Drag)"));
  Serial.println(F("  #MU:btn       : Soltar boton (Drop)"));
  Serial.println(F("  #MW:dz        : Rueda de desplazamiento (Scroll dz)"));

  Serial.println(F("\n[COMANDOS DE PROTOCOLO Y CONTROL]"));
  Serial.println(F("  #PING / #SYNC : Sincronizacion con App (#PONG:BIOS_KEYBOARD_V2:LINK=...)"));
  Serial.println(F("  #STATUS       : Estado maquina (#STAT:LINK=1,ADDR=...,CFG=...,PKTS=...)"));
  Serial.println(F("  #TEXTO        : Activa modo texto (escribe cualquier caracter)"));
  Serial.println(F("  #BIOS         : Activa modo BIOS rapido (1 tecla directa)"));
  Serial.println(F("  #AYUDA        : Muestra este menu"));
  Serial.println(F("  #TEST         : Ejecuta el test electrico de pines USB"));
  Serial.println(F("  #RESET        : Fuerza reinicio del bus USB SE0"));
  Serial.println(F("  ?             : Estado de conexion con la PC"));
  
  Serial.println(F("\n[TECLAS ESPECIALES - Escribe entre corchetes]"));
  Serial.println(F("  Funciones  : [F1] hasta [F12]"));
  Serial.println(F("  Flechas    : [UP], [DOWN], [LEFT], [RIGHT]"));
  Serial.println(F("  Control    : [ENTER], [ESC], [DEL] o [SUPR], [BS], [TAB]"));
  Serial.println(F("  Navegacion : [HOME], [END], [PGUP], [PGDN], [INS]"));
  Serial.println(F("  Atajos     : [CAD] (Ctrl+Alt+Del), [WIN], [CAPS]"));
  Serial.println(F("               [CTRL+C], [CTRL+V], [CTRL+Z], [ALT+F4]"));
  
  if (currentMode == MODE_QUICK_BIOS) {
    Serial.println(F("\n[TECLAS DIRECTAS EN MODO BIOS]"));
    Serial.println(F("  s=SUPR (burst) | 2=F2 (burst) | f=F11 (burst) | b=F12 (burst)"));
    Serial.println(F("  u=Arriba | d=Abajo | l=Izquierda | r=Derecha"));
    Serial.println(F("  e=ENTER | x=ESC | 0=F10 | +=Mas | -=Menos | Espacio=Espacio"));
  }
  Serial.println(F("============================================================\n"));
}

void printStatus() {
  Serial.println(F("\n=== ESTADO DE CONEXION CON LA PC ==="));
  bool link = UsbKeyboard.isConnectedToHost();
  uint8_t cfg = UsbKeyboard.getUsbConfiguration();
  uint8_t addr = UsbKeyboard.getUsbAddress();
  uint16_t pkts = UsbKeyboard.getPacketCount();

  if (link || pkts > 0) {
    Serial.print(F("PC COMUNICADA: SI | Config: "));
    Serial.print(cfg);
    Serial.print(F(" | Direccion USB: "));
    Serial.print(addr);
    Serial.print(F(" | Paquetes: "));
    Serial.println(pkts);
    Serial.println(F("-> Teclado + Raton listos y operativos."));
  } else {
    Serial.println(F("PC COMUNICADA: NO (Sin comunicacion USB)"));
    Serial.println(F("-> Envia '#TEST' para probar el cable USB."));
    Serial.println(F("-> Envia '#RESET' para forzar reconexion."));
  }
  Serial.println(F("====================================\n"));
}

void testHardwarePins() {
  Serial.println(F("\n=== TEST DE PINES USB (HARDWARE) ==="));
  cli();
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  pinMode(2, INPUT);
  digitalWrite(2, LOW);
  pinMode(4, INPUT);
  digitalWrite(4, LOW);
  delay(30);

  int valDPlus = digitalRead(2);
  int valDMinus = digitalRead(4);

  Serial.print(F("Pin D4 (D- / Blanco): "));
  Serial.println(valDMinus ? F("ALTO [PULL-UP OK]") : F("BAJO [FALLO: NO HAY PULL-UP]"));
  Serial.print(F("Pin D2 (D+ / Verde):  "));
  Serial.println(valDPlus ? F("ALTO [ERROR: DEBE SER BAJO]") : F("BAJO [OK]"));

  if (valDMinus == HIGH && valDPlus == LOW) {
    Serial.println(F("-> CIRCUITO ELECTRICO: CORRECTO (Polarizacion Low-Speed OK)"));
  } else if (valDMinus == LOW && valDPlus == HIGH) {
    Serial.println(F("-> ERROR: Cables Verde y Blanco INVERTIDOS!"));
  } else if (valDMinus == LOW && valDPlus == LOW) {
    Serial.println(F("-> ERROR: Falta resistencia 1.5k a 3.3V en cable blanco!"));
  }

  usbInit();
  sei();
  Serial.println(F("====================================\n"));
}

void reconnectUSB() {
  Serial.println(F("\n[REINICIANDO BUS USB SE0]..."));
  UsbKeyboard.init();
  for (int i = 0; i < 400; i++) {
    UsbKeyboard.update();
    delayMicroseconds(1000);
  }
  printStatus();
}

// ---------------------------------------------------------------------------
// Envío de teclas USB HID con ráfagas o pulso único antirrebote
// ---------------------------------------------------------------------------
bool sendKeyBurst(uint8_t keycode, uint8_t repeats, const char* name = NULL, uint8_t modifiers = 0) {
  // Para teclas de navegación o ENTER de 1 pulsación, aplicar antirrebote
  if (repeats == 1 && shouldDebounce(keycode)) {
    return true; // Rebote ignorado silenciosamente
  }

  bool anyDelivered = false;

  for (uint8_t r = 0; r < repeats; r++) {
    digitalWrite(LED_BUILTIN, HIGH);
    bool delivered = UsbKeyboard.sendKeyStroke(keycode, modifiers);
    if (delivered) anyDelivered = true;

    for (int i = 0; i < 5; i++) {
      UsbKeyboard.update();
      delayMicroseconds(4000);
    }
    digitalWrite(LED_BUILTIN, LOW);
  }

  // Reflejar estado de Link en LED
  if (UsbKeyboard.isConnectedToHost()) {
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }

  if (name != NULL) {
    Serial.print(F("OK: "));
    Serial.print(name);
    if (anyDelivered) {
      Serial.println(F(" [PC RECIBIO OK]"));
    } else {
      Serial.println(F(" [PC NO RESPONDE]"));
    }
  }
  return anyDelivered;
}

// ---------------------------------------------------------------------------
// Envío de carácter individual en Modo Texto
// ---------------------------------------------------------------------------
bool sendSingleChar(char c) {
  uint8_t keycode = 0;
  uint8_t mod = 0;

  if (c >= 'a' && c <= 'z') {
    keycode = KEY_A + (c - 'a');
  } else if (c >= 'A' && c <= 'Z') {
    keycode = KEY_A + (c - 'A');
    mod = MOD_SHIFT_LEFT;
  } else if (c >= '1' && c <= '9') {
    keycode = KEY_1 + (c - '1');
  } else if (c == '0') {
    keycode = KEY_0;
  } else if (c == ' ') {
    keycode = KEY_SPACE;
  } else if (c == '\n' || c == '\r') {
    if (shouldDebounce(KEY_ENTER)) {
      return true; // Evitar doble Enter si la terminal envía \r\n
    }
    keycode = KEY_ENTER;
  } else if (c == '\t') {
    keycode = HID_KEY_TAB;
  } else if (c == 8 || c == 127) {
    keycode = HID_KEY_BACKSPACE;
  } else {
    switch (c) {
      case '-': keycode = HID_KEY_MINUS; break;
      case '_': keycode = HID_KEY_MINUS; mod = MOD_SHIFT_LEFT; break;
      case '=': keycode = HID_KEY_EQUAL; break;
      case '+': keycode = HID_KEY_EQUAL; mod = MOD_SHIFT_LEFT; break;
      case '[': keycode = HID_KEY_LEFTBRACE; break;
      case '{': keycode = HID_KEY_LEFTBRACE; mod = MOD_SHIFT_LEFT; break;
      case ']': keycode = HID_KEY_RIGHTBRACE; break;
      case '}': keycode = HID_KEY_RIGHTBRACE; mod = MOD_SHIFT_LEFT; break;
      case '\\': keycode = HID_KEY_BACKSLASH; break;
      case '|': keycode = HID_KEY_BACKSLASH; mod = MOD_SHIFT_LEFT; break;
      case ';': keycode = HID_KEY_SEMICOLON; break;
      case ':': keycode = HID_KEY_SEMICOLON; mod = MOD_SHIFT_LEFT; break;
      case '\'': keycode = HID_KEY_APOSTROPHE; break;
      case '"': keycode = HID_KEY_APOSTROPHE; mod = MOD_SHIFT_LEFT; break;
      case '`': keycode = HID_KEY_GRAVE; break;
      case '~': keycode = HID_KEY_GRAVE; mod = MOD_SHIFT_LEFT; break;
      case ',': keycode = HID_KEY_COMMA; break;
      case '<': keycode = HID_KEY_COMMA; mod = MOD_SHIFT_LEFT; break;
      case '.': keycode = HID_KEY_DOT; break;
      case '>': keycode = HID_KEY_DOT; mod = MOD_SHIFT_LEFT; break;
      case '/': keycode = HID_KEY_SLASH; break;
      case '?': keycode = HID_KEY_SLASH; mod = MOD_SHIFT_LEFT; break;
      case '!': keycode = KEY_1; mod = MOD_SHIFT_LEFT; break;
      case '@': keycode = KEY_2; mod = MOD_SHIFT_LEFT; break;
      case '#': keycode = KEY_3; mod = MOD_SHIFT_LEFT; break;
      case '$': keycode = KEY_4; mod = MOD_SHIFT_LEFT; break;
      case '%': keycode = KEY_5; mod = MOD_SHIFT_LEFT; break;
      case '^': keycode = KEY_6; mod = MOD_SHIFT_LEFT; break;
      case '&': keycode = KEY_7; mod = MOD_SHIFT_LEFT; break;
      case '*': keycode = KEY_8; mod = MOD_SHIFT_LEFT; break;
      case '(': keycode = KEY_9; mod = MOD_SHIFT_LEFT; break;
      case ')': keycode = KEY_0; mod = MOD_SHIFT_LEFT; break;
      default: return false;
    }
  }

  if (keycode != 0) {
    digitalWrite(LED_BUILTIN, HIGH);
    bool ok = UsbKeyboard.sendKeyStroke(keycode, mod);
    digitalWrite(LED_BUILTIN, LOW);

    for (int i = 0; i < 3; i++) {
      UsbKeyboard.update();
      delayMicroseconds(2500);
    }
    return ok;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Procesa etiquetas de teclas especiales entre corchetes: ej. [F2], [DEL], [UP]
// ---------------------------------------------------------------------------
bool processSpecialTag(const char* tag) {
  char upper[24];
  uint8_t len = 0;
  while (tag[len] != '\0' && len < 23) {
    upper[len] = toupper(tag[len]);
    len++;
  }
  upper[len] = '\0';

  // Teclas F1 - F12 (F2, F11, F12 llevan ráfaga para POST)
  if (strcmp(upper, "F1") == 0)  return sendKeyBurst(KEY_F1, 1, "F1");
  if (strcmp(upper, "F2") == 0)  return sendKeyBurst(KEY_F2, 6, "F2 (BURST)");
  if (strcmp(upper, "F3") == 0)  return sendKeyBurst(KEY_F3, 1, "F3");
  if (strcmp(upper, "F4") == 0)  return sendKeyBurst(KEY_F4, 1, "F4");
  if (strcmp(upper, "F5") == 0)  return sendKeyBurst(KEY_F5, 1, "F5");
  if (strcmp(upper, "F6") == 0)  return sendKeyBurst(KEY_F6, 1, "F6");
  if (strcmp(upper, "F7") == 0)  return sendKeyBurst(KEY_F7, 1, "F7");
  if (strcmp(upper, "F8") == 0)  return sendKeyBurst(KEY_F8, 1, "F8");
  if (strcmp(upper, "F9") == 0)  return sendKeyBurst(KEY_F9, 1, "F9");
  if (strcmp(upper, "F10") == 0) return sendKeyBurst(KEY_F10, 1, "F10");
  if (strcmp(upper, "F11") == 0) return sendKeyBurst(KEY_F11, 6, "F11 (BURST)");
  if (strcmp(upper, "F12") == 0) return sendKeyBurst(KEY_F12, 6, "F12 (BURST)");

  // Flechas de dirección (1 pulsación debounced)
  if (strcmp(upper, "UP") == 0 || strcmp(upper, "ARRIBA") == 0)
    return sendKeyBurst(KEY_UP_ARROW, 1, "ARRIBA");
  if (strcmp(upper, "DOWN") == 0 || strcmp(upper, "ABAJO") == 0)
    return sendKeyBurst(KEY_DOWN_ARROW, 1, "ABAJO");
  if (strcmp(upper, "LEFT") == 0 || strcmp(upper, "IZQUIERDA") == 0 || strcmp(upper, "IZQ") == 0)
    return sendKeyBurst(KEY_LEFT_ARROW, 1, "IZQUIERDA");
  if (strcmp(upper, "RIGHT") == 0 || strcmp(upper, "DERECHA") == 0 || strcmp(upper, "DER") == 0)
    return sendKeyBurst(KEY_RIGHT_ARROW, 1, "DERECHA");

  // Control y edición (1 pulsación debounced)
  if (strcmp(upper, "DEL") == 0 || strcmp(upper, "SUPR") == 0 || strcmp(upper, "DELETE") == 0)
    return sendKeyBurst(KEY_DELETE, 6, "SUPR (BURST)");
  if (strcmp(upper, "ENTER") == 0 || strcmp(upper, "ENTRAR") == 0)
    return sendKeyBurst(KEY_ENTER, 1, "ENTER");
  if (strcmp(upper, "ESC") == 0 || strcmp(upper, "ESCAPE") == 0)
    return sendKeyBurst(KEY_ESC, 1, "ESC");
  if (strcmp(upper, "BS") == 0 || strcmp(upper, "BACKSPACE") == 0)
    return sendKeyBurst(HID_KEY_BACKSPACE, 1, "BACKSPACE");
  if (strcmp(upper, "TAB") == 0)
    return sendKeyBurst(HID_KEY_TAB, 1, "TAB");
  if (strcmp(upper, "SPACE") == 0 || strcmp(upper, "ESPACIO") == 0)
    return sendKeyBurst(KEY_SPACE, 1, "ESPACIO");

  // Teclas de navegación
  if (strcmp(upper, "HOME") == 0 || strcmp(upper, "INICIO") == 0)
    return sendKeyBurst(KEY_HOME, 1, "INICIO");
  if (strcmp(upper, "END") == 0 || strcmp(upper, "FIN") == 0)
    return sendKeyBurst(KEY_END, 1, "FIN");
  if (strcmp(upper, "PGUP") == 0 || strcmp(upper, "REPAG") == 0)
    return sendKeyBurst(KEY_PAGEUP, 1, "RE PAG");
  if (strcmp(upper, "PGDN") == 0 || strcmp(upper, "AVPAG") == 0)
    return sendKeyBurst(KEY_PAGEDOWN, 1, "AV PAG");
  if (strcmp(upper, "INS") == 0 || strcmp(upper, "INSERT") == 0)
    return sendKeyBurst(KEY_INSERT, 1, "INSERT");
  if (strcmp(upper, "PRTSC") == 0)
    return sendKeyBurst(KEY_PRINTSCREEN, 1, "PRINT SCREEN");
  if (strcmp(upper, "CAPS") == 0 || strcmp(upper, "BLOQMAYUS") == 0)
    return sendKeyBurst(HID_KEY_CAPSLOCK, 1, "CAPS LOCK");
  if (strcmp(upper, "WIN") == 0 || strcmp(upper, "GUI") == 0)
    return sendKeyBurst(0, 1, "TECLA WINDOWS", MOD_GUI_LEFT);

  // Atajos compuestos comunes
  if (strcmp(upper, "CAD") == 0 || strcmp(upper, "CTRL+ALT+DEL") == 0 || strcmp(upper, "CTRL+ALT+SUPR") == 0)
    return sendKeyBurst(KEY_DELETE, 2, "CTRL+ALT+DEL", MOD_CONTROL_LEFT | MOD_ALT_LEFT);
  if (strcmp(upper, "ALT+F4") == 0)
    return sendKeyBurst(KEY_F4, 2, "ALT+F4", MOD_ALT_LEFT);
  if (strcmp(upper, "CTRL+C") == 0)
    return sendKeyBurst(KEY_C, 1, "CTRL+C", MOD_CONTROL_LEFT);
  if (strcmp(upper, "CTRL+V") == 0)
    return sendKeyBurst(KEY_V, 1, "CTRL+V", MOD_CONTROL_LEFT);
  if (strcmp(upper, "CTRL+Z") == 0)
    return sendKeyBurst(KEY_Z, 1, "CTRL+Z", MOD_CONTROL_LEFT);
  if (strcmp(upper, "CTRL+A") == 0)
    return sendKeyBurst(KEY_A, 1, "CTRL+A", MOD_CONTROL_LEFT);
  if (strcmp(upper, "CTRL+X") == 0)
    return sendKeyBurst(KEY_X, 1, "CTRL+X", MOD_CONTROL_LEFT);

  // Si no se reconoce la etiqueta, enviarla como texto literal
  sendSingleChar('[');
  for (uint8_t i = 0; tag[i] != '\0'; i++) {
    sendSingleChar(tag[i]);
  }
  sendSingleChar(']');
  return true;
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Pin 5 en alto por si se conecta la resistencia de pull-up allí
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // Desactivar interrupción Timer0 para estabilidad en timing USB bit-banging
  TIMSK0 &= ~(1 << TOIE0);

  // Inicializar temporizador de hardware Timer1 para debounce sin interrupciones
  initDebounceTimer();

  Serial.begin(9600);

  // Inicializar bus USB
  UsbKeyboard.init();

  for (int i = 0; i < 400; i++) {
    UsbKeyboard.update();
    delayMicroseconds(1000);
  }

  Serial.println(F("\n==========================================================="));
  Serial.println(F("ARDUINO USB KEYBOARD + MOUSE v2.0 (TECLADO + RATON + BIOS)"));
  Serial.println(F("==========================================================="));
  printStatus();
  printHelp();
}

// ---------------------------------------------------------------------------
// Loop Principal
// ---------------------------------------------------------------------------
void loop() {
  UsbKeyboard.update();

  // Alerta automática cuando la PC se conecte o desconecte
  static bool lastConnected = false;
  bool currentConnected = UsbKeyboard.isConnectedToHost();
  if (currentConnected && !lastConnected) {
    Serial.println(F("\n*** [EXITO] ¡LA PC HA RECONOCIDO EL TECLADO Y RATON! ***\n"));
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (!currentConnected && lastConnected) {
    Serial.println(F("\n*** [AVISO] LA PC SE HA DESCONECTADO ***\n"));
    digitalWrite(LED_BUILTIN, LOW);
  }
  lastConnected = currentConnected;

  // Procesamiento de entrada Serial
  static char tagBuffer[32];
  static uint8_t tagIdx = 0;
  static bool insideTag = false;

  static char cmdBuffer[32];
  static uint8_t cmdIdx = 0;
  static bool insideCmd = false;

  static bool skipNextLF = false;

  while (Serial.available() > 0) {
    char c = Serial.read();

    // Si el delimitador anterior fue '\r', ignorar el '\n' inmediatamente posterior
    if (skipNextLF) {
      skipNextLF = false;
      if (c == '\n') continue;
    }

    // Detección de comandos con '#' (ej. #PING, #SYNC, #STATUS, #M:dx,dy, #ML, #MR, etc.)
    if (!insideTag && !insideCmd && c == '#') {
      insideCmd = true;
      cmdIdx = 0;
      continue;
    }

    if (insideCmd) {
      if (c == '\n' || c == '\r' || c == ' ') {
        insideCmd = false;
        if (c == '\r') skipNextLF = true;
        cmdBuffer[cmdIdx] = '\0';

        // Convertir a mayúsculas
        for (uint8_t i = 0; cmdBuffer[i]; i++) cmdBuffer[i] = toupper(cmdBuffer[i]);

        // Protocolo de sincronización con aplicación (Versión 2.0)
        if (strcmp(cmdBuffer, "PING") == 0 || strcmp(cmdBuffer, "SYNC") == 0) {
          Serial.print(F("#PONG:BIOS_KEYBOARD_V2:LINK="));
          Serial.print(UsbKeyboard.isConnectedToHost() ? '1' : '0');
          Serial.print(F(",PKTS="));
          Serial.println(UsbKeyboard.getPacketCount());
        } else if (strcmp(cmdBuffer, "STATUS") == 0) {
          if (UsbKeyboard.isConnectedToHost()) {
            Serial.print(F("#STAT:LINK=1,ADDR="));
            Serial.print(UsbKeyboard.getUsbAddress());
            Serial.print(F(",CFG="));
            Serial.print(UsbKeyboard.getUsbConfiguration());
            Serial.print(F(",PKTS="));
            Serial.println(UsbKeyboard.getPacketCount());
          } else {
            Serial.println(F("#STAT:LINK=0"));
          }
        }
        // Comandos de Emulación de Ratón (Mouse HID)
        else if (strncmp(cmdBuffer, "M:", 2) == 0) {
          // #M:dx,dy (ej. #M:15,-20)
          char* comma = strchr(cmdBuffer + 2, ',');
          if (comma != NULL) {
            *comma = '\0';
            int dx = atoi(cmdBuffer + 2);
            int dy = atoi(comma + 1);
            UsbKeyboard.mouseMove((int8_t)dx, (int8_t)dy);
            UsbKeyboard.update();
          }
        } else if (strcmp(cmdBuffer, "ML") == 0) {
          UsbKeyboard.mouseClick(1);
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE CLICK LEFT"));
        } else if (strcmp(cmdBuffer, "MR") == 0) {
          UsbKeyboard.mouseClick(2);
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE CLICK RIGHT"));
        } else if (strcmp(cmdBuffer, "MM") == 0) {
          UsbKeyboard.mouseClick(4);
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE CLICK MIDDLE"));
        } else if (strncmp(cmdBuffer, "MB:", 3) == 0) {
          int btn = atoi(cmdBuffer + 3);
          UsbKeyboard.mouseClick((uint8_t)(btn > 0 ? btn : 1));
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE BUTTON CLICK"));
        } else if (strncmp(cmdBuffer, "MD", 2) == 0) {
          // #MD:btn o #MD (pulsar y mantener para arrastre)
          uint8_t btn = 1;
          if (cmdBuffer[2] == ':') {
            if (cmdBuffer[3] == 'R' || cmdBuffer[3] == '2') btn = 2;
            else if (cmdBuffer[3] == 'M' || cmdBuffer[3] == '4') btn = 4;
            else if (cmdBuffer[3] == 'L' || cmdBuffer[3] == '1') btn = 1;
            else btn = atoi(cmdBuffer + 3);
          }
          UsbKeyboard.mousePress(btn > 0 ? btn : 1);
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE BUTTON DOWN"));
        } else if (strncmp(cmdBuffer, "MU", 2) == 0) {
          // #MU:btn o #MU (soltar botón)
          uint8_t btn = 0xFF; // soltar todos si no se especifica
          if (cmdBuffer[2] == ':') {
            if (cmdBuffer[3] == 'R' || cmdBuffer[3] == '2') btn = 2;
            else if (cmdBuffer[3] == 'M' || cmdBuffer[3] == '4') btn = 4;
            else if (cmdBuffer[3] == 'L' || cmdBuffer[3] == '1') btn = 1;
            else btn = atoi(cmdBuffer + 3);
          }
          UsbKeyboard.mouseRelease(btn);
          UsbKeyboard.update();
          Serial.println(F("OK: MOUSE BUTTON UP"));
        } else if (strncmp(cmdBuffer, "MW:", 3) == 0) {
          int dz = atoi(cmdBuffer + 3);
          UsbKeyboard.mouseScroll((int8_t)dz);
          UsbKeyboard.update();
        }
        // Comandos de modo y diagnóstico originales
        else if (strcmp(cmdBuffer, "BIOS") == 0 || strcmp(cmdBuffer, "MODO2") == 0) {
          currentMode = MODE_QUICK_BIOS;
          Serial.println(F("\n-> MODO CAMBIADO: MODO BIOS RAPIDO (s=Supr, u=Arriba, etc.)"));
        } else if (strcmp(cmdBuffer, "TEXTO") == 0 || strcmp(cmdBuffer, "MODO1") == 0) {
          currentMode = MODE_FULL_TEXT;
          Serial.println(F("\n-> MODO CAMBIADO: MODO TEXTO COMPLETO (Escribe libremente)"));
        } else if (strcmp(cmdBuffer, "AYUDA") == 0 || strcmp(cmdBuffer, "HELP") == 0) {
          printHelp();
        } else if (strcmp(cmdBuffer, "TEST") == 0) {
          testHardwarePins();
        } else if (strcmp(cmdBuffer, "RESET") == 0) {
          reconnectUSB();
        } else {
          Serial.print(F("Comando desconocido: #"));
          Serial.println(cmdBuffer);
        }
        continue;
      } else {
        if (cmdIdx < 30) {
          cmdBuffer[cmdIdx++] = c;
        }
        continue;
      }
    }

    // Consulta rápida de estado con '?'
    if (!insideTag && c == '?') {
      printStatus();
      continue;
    }

    // Inicio de etiqueta especial '['
    if (c == '[') {
      insideTag = true;
      tagIdx = 0;
      continue;
    }

    // Fin de etiqueta especial ']'
    if (insideTag) {
      if (c == ']') {
        insideTag = false;
        tagBuffer[tagIdx] = '\0';
        processSpecialTag(tagBuffer);
      } else {
        if (tagIdx < 30) {
          tagBuffer[tagIdx++] = c;
        } else {
          // Desbordamiento: descartar y procesar lo recibido
          insideTag = false;
          tagBuffer[tagIdx] = '\0';
          processSpecialTag(tagBuffer);
        }
      }
      continue;
    }

    // MODO BIOS RAPIDO:
    if (currentMode == MODE_QUICK_BIOS) {
      switch (c) {
        case 's': case 'S': sendKeyBurst(KEY_DELETE, 6, "SUPR"); break;
        case '2':           sendKeyBurst(KEY_F2, 6, "F2"); break;
        case 'f': case 'F': sendKeyBurst(KEY_F11, 6, "F11"); break;
        case 'b': case 'B': sendKeyBurst(KEY_F12, 6, "F12"); break;
        case '0':           sendKeyBurst(KEY_F10, 1, "F10"); break;
        case 'u': case 'U': sendKeyBurst(KEY_UP_ARROW, 1, "ARRIBA"); break;
        case 'd': case 'D': sendKeyBurst(KEY_DOWN_ARROW, 1, "ABAJO"); break;
        case 'l': case 'L': sendKeyBurst(KEY_LEFT_ARROW, 1, "IZQUIERDA"); break;
        case 'r': case 'R': sendKeyBurst(KEY_RIGHT_ARROW, 1, "DERECHA"); break;
        case 'e': case 'E': sendKeyBurst(KEY_ENTER, 1, "ENTER"); break;
        case 'x': case 'X': sendKeyBurst(KEY_ESC, 1, "ESC"); break;
        case ' ':           sendKeyBurst(KEY_SPACE, 1, "ESPACIO"); break;
        case '+':           sendKeyBurst(HID_KEY_EQUAL, 1, "+", MOD_SHIFT_LEFT); break;
        case '-':           sendKeyBurst(HID_KEY_MINUS, 1, "-"); break;
        case '\n': case '\r': break; // Ignorar saltos en modo BIOS
        default:
          sendSingleChar(c);
          break;
      }
    } else {
      // MODO TEXTO COMPLETO:
      sendSingleChar(c);
    }
  }
}
