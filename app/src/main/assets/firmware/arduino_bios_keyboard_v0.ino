/*
 * Arduino UNO USB HID Keyboard para BIOS / UEFI vía Serial
 * Con Diagnóstico de Conexión en Tiempo Real y Test de Hardware
 */

#include "UsbKeyboard.h"

// Códigos USB HID estándar
#define HID_KEY_ENTER       40   // Enter
#define HID_KEY_ESC         41   // Escape
#define HID_KEY_SPACE       44   // Espacio
#define HID_KEY_F2          59   // F2 (BIOS Asus / Dell / Acer)
#define HID_KEY_F10         67   // F10 (Guardar y Salir)
#define HID_KEY_F11         68   // F11 (Boot Menu)
#define HID_KEY_F12         69   // F12 (Boot Menu alternativo)
#define HID_KEY_DELETE      76   // Supr / Del (BIOS estándar)
#define HID_KEY_RIGHT_ARROW 79   // Derecha
#define HID_KEY_LEFT_ARROW  80   // Izquierda
#define HID_KEY_DOWN_ARROW  81   // Abajo
#define HID_KEY_UP_ARROW    82   // Arriba

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
    Serial.print(F(" | Paquetes recibidos: "));
    Serial.println(pkts);
    Serial.println(F("-> La BIOS/PC reconoce el teclado correctamente."));
  } else {
    Serial.println(F("PC COMUNICADA: NO (Sin comunicacion USB)"));
    Serial.println(F("-> La PC aun no detecta el teclado."));
    Serial.println(F("-> Envia 't' para ejecutar el TEST de pines de hardware."));
    Serial.println(F("-> Envia 'w' para forzar reinicio del bus USB."));
  }
  Serial.println(F("===================================\n"));
}

void testHardwarePins() {
  Serial.println(F("\n=== TEST DE PINES USB (DIAGNOSTICO HARDWARE) ==="));
  
  // Desactivar temporalmente interrupciones para medir pines en reposo
  cli();
  
  // Asegurar que Pin 5 entregue 5V por si el pullup está en D5
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // Configurar Pin 2 (D+) y Pin 4 (D-) como entradas flotantes (sin pull-up interno)
  pinMode(2, INPUT);
  digitalWrite(2, LOW);
  pinMode(4, INPUT);
  digitalWrite(4, LOW);
  
  delay(30);
  
  int valDPlus = digitalRead(2);
  int valDMinus = digitalRead(4);
  
  Serial.print(F("1) Pin D4 (D- / Cable Blanco): "));
  if (valDMinus == HIGH) {
    Serial.println(F("ALTO (3.3V-5V) [PULL-UP OK]"));
  } else {
    Serial.println(F("BAJO (0V) [FALLO: NO HAY PULL-UP]"));
  }

  Serial.print(F("2) Pin D2 (D+ / Cable Verde):  "));
  if (valDPlus == HIGH) {
    Serial.println(F("ALTO (3.3V-5V) [PELIGRO: DEBERIA ESTAR EN 0V EN REPOSO]"));
  } else {
    Serial.println(F("BAJO (0V) [OK]"));
  }

  Serial.println(F("\n--- DIAGNOSTICO ---"));
  if (valDMinus == HIGH && valDPlus == LOW) {
    Serial.println(F("-> ESTADO ELECTRICO: CORRECTO (Polarizacion Low-Speed detectada)."));
    Serial.println(F("   Si la PC sigue sin reconocerlo, envia 'w' para reiniciar el bus"));
    Serial.println(F("   o verifica que los diodos Zener sean de 3.6V y el cable Negro a GND."));
  } else if (valDMinus == LOW && valDPlus == HIGH) {
    Serial.println(F("-> ¡ERROR CRITICO: CABLES INVERTIDOS!"));
    Serial.println(F("   El cable Verde (D+) y Blanco (D-) estan cambiados de pin."));
    Serial.println(F("   Solucion: Conecta Verde a D2 y Blanco a D4."));
  } else if (valDMinus == LOW && valDPlus == LOW) {
    Serial.println(F("-> ¡ERROR CRITICO: FALTA LA RESISTENCIA PULL-UP DE 1.5k!"));
    Serial.println(F("   El pin D4 esta en 0V. La PC NUNCA detectara conexion si D- esta en 0V."));
    Serial.println(F("   Solucion: Conecta una resistencia de 1.5k ohm entre el"));
    Serial.println(F("   cable Blanco (D-/Pin 4) y el pin 3.3V (o pin D5) del Arduino."));
  } else if (valDMinus == HIGH && valDPlus == HIGH) {
    Serial.println(F("-> ERROR: Cortocircuito detectado entre D+ y D-."));
    Serial.println(F("   Revisa que los cables Verde y Blanco no se esten tocando."));
  }

  // Reactivar USB
  usbInit();
  sei();
  Serial.println(F("================================================\n"));
}

void reconnectUSB() {
  Serial.println(F("\n[FORZANDO REINICIO USB SE0]..."));
  UsbKeyboard.init();
  for (int i = 0; i < 400; i++) {
    UsbKeyboard.update();
    delayMicroseconds(1000);
  }
  printStatus();
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Asegurar Pin 5 en HIGH por si el pull-up se conecta a D5
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);

  // Desactivar interrupción de Timer0 para estabilidad en timing V-USB
  TIMSK0 &= ~(1 << TOIE0);

  // Inicializar puerto Serie a 9600 baudios
  Serial.begin(9600);

  // Reiniciar bus USB limpiamente con el protocolo nativo V-USB
  UsbKeyboard.init();

  // Bucle de calentamiento inicial para responder a los primeros descriptores
  for (int i = 0; i < 400; i++) {
    UsbKeyboard.update();
    delayMicroseconds(1000);
  }

  Serial.println(F("OK: Teclado BIOS USB Listo"));
  printStatus();
}

// Envía ráfagas repetidas para garantizar captura en POST de BIOS
void sendKeyBurst(uint8_t keycode, uint8_t repeats, const char* name = NULL, uint8_t modifiers = 0) {
  bool anyDelivered = false;

  for (uint8_t r = 0; r < repeats; r++) {
    // Parpadeo de LED en cada pulso
    digitalWrite(LED_BUILTIN, HIGH);
    
    bool delivered = UsbKeyboard.sendKeyStroke(keycode, modifiers);
    if (delivered) anyDelivered = true;

    // Margen de lectura para el BIOS (~24 ms)
    for (int i = 0; i < 6; i++) {
      UsbKeyboard.update();
      delayMicroseconds(4000);
    }
    
    digitalWrite(LED_BUILTIN, LOW);
  }

  // Reflejar estado de Link en el LED: encendido fijo si la PC está conectada
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
      Serial.println(F(" [PC NO RESPONDE - REVISA CABLE/PULLUP]"));
    }
  }
}

void loop() {
  UsbKeyboard.update(); // Mantiene vivo el protocolo USB

  // Notificación reactiva cuando la PC se conecte o reconozca el teclado
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

  if (Serial.available() > 0) {
    char cmd = Serial.read();
    switch (cmd) {
      // Diagnóstico manual de estado con '?' o 'c'
      case '?':
      case 'c':
      case 'C':
        printStatus();
        break;

      // Test de pines de hardware con 't'
      case 't':
      case 'T':
        testHardwarePins();
        break;

      // Forzar reconexión USB con 'w'
      case 'w':
      case 'W':
        reconnectUSB();
        break;

      // Teclas de acceso a BIOS (Ráfaga de 6 pulsaciones)
      case 's':
      case 'S':
        sendKeyBurst(HID_KEY_DELETE, 6, "SUPR");
        break;

      case '2':
        sendKeyBurst(HID_KEY_F2, 6, "F2");
        break;

      case 'f':
      case 'F':
        sendKeyBurst(HID_KEY_F11, 6, "F11");
        break;

      case 'b':
      case 'B':
        sendKeyBurst(HID_KEY_F12, 6, "F12");
        break;

      // Teclas de navegación interna (1 pulsación limpia para no saltar ni cerrar menús)
      case 'u':
      case 'U':
        sendKeyBurst(HID_KEY_UP_ARROW, 1, "ARRIBA");
        break;

      case 'd':
      case 'D':
        sendKeyBurst(HID_KEY_DOWN_ARROW, 1, "ABAJO");
        break;

      case 'l':
      case 'L':
        sendKeyBurst(HID_KEY_LEFT_ARROW, 1, "IZQUIERDA");
        break;

      case 'r':
      case 'R':
        sendKeyBurst(HID_KEY_RIGHT_ARROW, 1, "DERECHA");
        break;

      case 'e':
      case 'E':
        sendKeyBurst(HID_KEY_ENTER, 1, "ENTER");
        break;

      case 'x':
      case 'X':
        sendKeyBurst(HID_KEY_ESC, 1, "ESC");
        break;

      case '0':
        sendKeyBurst(HID_KEY_F10, 1, "F10");
        break;

      case ' ':
        sendKeyBurst(HID_KEY_SPACE, 1, "ESPACIO");
        break;

      case '+':
        sendKeyBurst(46, 1, "+", (1<<1));
        break;

      case '-':
        sendKeyBurst(45, 1, "-");
        break;
    }
  }
}
