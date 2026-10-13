/*
 * Arduino UNO USB HID Keyboard - VERSION 1.0 (Teclado Completo + BIOS)
 * Compatible con BIOS / UEFI, Boot Menu, GRUB, Windows, Linux y Shell.
 *
 * Características:
 * 1. MODO TEXTO (Por defecto): Escribe cualquier texto carácter por carácter
 *    (a-z, A-Z con Shift automático, 0-9, y todos los símbolos).
 * 2. COMANDOS DE TECLAS ESPECIALES: Usa etiquetas como [F1]-[F12], [UP], [DOWN],
 *    [LEFT], [RIGHT], [ENTER], [ESC], [DEL], [BS], [TAB], [SPACE], [CAD], etc.
 * 3. MODO BIOS RAPIDO: Escribe '#BIOS' para activar el modo de 1 tecla (s=Supr,
 *    u=Arriba, d=Abajo, e=Enter, 2=F2, f=F11, etc.).
 * 4. Diagnóstico de hardware en tiempo real con '#' o '?'.
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

void printHelp() {
  Serial.println(F("\n================ AYUDA TECLADO V1.0 ================"));
  Serial.println(F("MODO ACTUAL: "));
  Serial.println(currentMode == MODE_FULL_TEXT ? F("-> MODO TEXTO COMPLETO") : F("-> MODO BIOS RAPIDO"));
  Serial.println(F("\n[COMANDOS DE CAMBIO DE MODO]"));
  Serial.println(F("  #TEXTO   : Activa modo texto (escribe cualquier caracter)"));
  Serial.println(F("  #BIOS    : Activa modo BIOS rapido (1 tecla directa)"));
  Serial.println(F("  #AYUDA   : Muestra este menu"));
  Serial.println(F("  #TEST    : Ejecuta el test electrico de pines USB"));
  Serial.println(F("  #RESET   : Fuerza reinicio del bus USB SE0"));
  Serial.println(F("  ?        : Estado de conexion con la PC"));
  
  Serial.println(F("\n[TECLAS ESPECIALES - Escribe entre corchetes]"));
  Serial.println(F("  Funciones  : [F1] hasta [F12]"));
  Serial.println(F("  Flechas    : [UP], [DOWN], [LEFT], [RIGHT]"));
  Serial.println(F("  Control    : [ENTER], [ESC], [DEL] o [SUPR], [BS], [TAB]"));
  Serial.println(F("  Navegacion : [HOME], [END], [PGUP], [PGDN], [INS]"));
  Serial.println(F("  Atajos     : [CAD] (Ctrl+Alt+Del), [WIN], [CAPS]"));
  Serial.println(F("               [CTRL+C], [CTRL+V], [CTRL+Z], [ALT+F4]"));
  
  if (currentMode == MODE_QUICK_BIOS) {
    Serial.println(F("\n[TECLAS DIRECTAS EN MODO BIOS]"));
    Serial.println(F("  s=SUPR | 2=F2 | f=F11 | b=F12 | 0=F10"));
    Serial.println(F("  u=Arriba | d=Abajo | l=Izquierda | r=Derecha"));
    Serial.println(F("  e=ENTER | x=ESC | Espacio=Espacio"));
  }
  Serial.println(F("====================================================\n"));
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
    Serial.println(F("-> Teclado listo y operativo."));
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
    Serial.println(F("-> ERROR: Falta resistencia 1.5k a 5V en cable blanco!"));
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

// Envío con ráfagas y confirmación de entrega
bool sendKeyBurst(uint8_t keycode, uint8_t repeats, const char* name = NULL, uint8_t modifiers = 0) {
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

// Envío de carácter individual en Modo Texto
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
    static unsigned long lastEnterMs = 0;
    unsigned long now = millis();
    if (now - lastEnterMs < 80) {
      return true; // Evitar doble Enter si la terminal envía \r\n
    }
    lastEnterMs = now;
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

// Procesa etiquetas de teclas especiales entre corchetes: ej. [F2], [DEL], [UP]
bool processSpecialTag(const char* tag) {
  // Ignorar mayúsculas/minúsculas haciendo copia local en mayúsculas
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

  // Flechas de dirección (1 pulsación por comando para no saltar de más)
  if (strcmp(upper, "UP") == 0 || strcmp(upper, "ARRIBA") == 0)
    return sendKeyBurst(KEY_UP_ARROW, 1, "ARRIBA");
  if (strcmp(upper, "DOWN") == 0 || strcmp(upper, "ABAJO") == 0)
    return sendKeyBurst(KEY_DOWN_ARROW, 1, "ABAJO");
  if (strcmp(upper, "LEFT") == 0 || strcmp(upper, "IZQUIERDA") == 0)
    return sendKeyBurst(KEY_LEFT_ARROW, 1, "IZQUIERDA");
  if (strcmp(upper, "RIGHT") == 0 || strcmp(upper, "DERECHA") == 0)
    return sendKeyBurst(KEY_RIGHT_ARROW, 1, "DERECHA");

  // Control y edición (1 pulsación para que ENTER no abra y cierre spinners)
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

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Pin 5 en alto por si se conecta el pull-up allí
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // Desactivar interrupción Timer0 para estabilidad en timing USB
  TIMSK0 &= ~(1 << TOIE0);

  Serial.begin(9600);

  // Inicializar bus USB
  UsbKeyboard.init();

  for (int i = 0; i < 400; i++) {
    UsbKeyboard.update();
    delayMicroseconds(1000);
  }

  Serial.println(F("\n==================================================="));
  Serial.println(F("ARDUINO USB KEYBOARD v1.0 (TECLADO COMPLETO + BIOS)"));
  Serial.println(F("==================================================="));
  printStatus();
  printHelp();
}

void loop() {
  UsbKeyboard.update();

  // Alerta automática cuando la PC se conecte o desconecte
  static bool lastConnected = false;
  bool currentConnected = UsbKeyboard.isConnectedToHost();
  if (currentConnected && !lastConnected) {
    Serial.println(F("\n*** [EXITO] ¡LA PC HA RECONOCIDO EL TECLADO! ***\n"));
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

  while (Serial.available() > 0) {
    char c = Serial.read();

    // Detección de comandos con '#' (ej. #TEXTO, #BIOS, #AYUDA, #TEST, #RESET)
    if (!insideTag && !insideCmd && c == '#') {
      insideCmd = true;
      cmdIdx = 0;
      continue;
    }

    if (insideCmd) {
      if (c == '\n' || c == '\r' || c == ' ') {
        insideCmd = false;
        cmdBuffer[cmdIdx] = '\0';

        // Convertir a mayúsculas
        for (uint8_t i = 0; cmdBuffer[i]; i++) cmdBuffer[i] = toupper(cmdBuffer[i]);

        // Protocolo de sincronización con la App Android
        if (strcmp(cmdBuffer, "PING") == 0 || strcmp(cmdBuffer, "SYNC") == 0) {
          Serial.print(F("#PONG:BIOS_KEYBOARD_V1:LINK="));
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
        } else if (strcmp(cmdBuffer, "BIOS") == 0 || strcmp(cmdBuffer, "MODO2") == 0) {
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
          // Desbordamiento: descartar y escribir literal
          insideTag = false;
          tagBuffer[tagIdx] = '\0';
          processSpecialTag(tagBuffer);
        }
      }
      continue;
    }

    // Si estamos en MODO BIOS RAPIDO:
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
