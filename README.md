# Teclado BIOS Arduino ⌨️ 🤖

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![Platform](https://img.shields.io/badge/Platform-Android-green.svg)](https://www.android.com/)
[![Min SDK](https://img.shields.io/badge/Min%20SDK-21%20(Lollipop)-orange.svg)](https://developer.android.com)
[![Target SDK](https://img.shields.io/badge/Target%20SDK-34%20(Android%2014)-brightgreen.svg)](https://developer.android.com)
[![Hardware](https://img.shields.io/badge/Hardware-Arduino%20Uno%20%7C%20Nano%20%7C%20Pro%20Micro-red.svg)](https://www.arduino.cc/)

Una aplicación Android diseñada para convertir tu smartphone o tablet en un teclado físico virtual y consola de control para acceder a la **BIOS / UEFI**, sistemas operativos heredados o terminales embebidas mediante una placa **Arduino** conectada por **USB OTG**.

---

## 📋 Tabla de Contenido
- [Características Principales](#-características-principales)
- [Arquitectura del Sistema](#-arquitectura-del-sistema)
- [Diagrama de Conexión de Hardware](#-diagrama-de-conexión-de-hardware)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Requisitos Previos](#-requisitos-previos)
- [Instalación y Configuración del SDK](#-instalación-y-configuración-del-sdk)
- [Compilación y Construcción](#-compilación-y-construcción)
- [Guía de Uso](#-guía-de-uso)
- [Optimización de Disco y Entorno](#-optimización-de-disco-y-entorno)
- [Licencia](#-licencia)

---

## ✨ Características Principales

- **Conexión Directa USB OTG:** Comunicación serie de baja latencia con Arduino mediante la biblioteca `usb-serial-for-android` (compatible con CH340, CP2102, FT232, PL2303 y CDC-ACM nativo).
- **Teclas Dedicadas para BIOS/UEFI:** Botones táctiles específicos para navegación rápida: `DEL`, `F1`-`F12`, `ESC`, `ENTER`, flechas direccionales (`UP`, `DOWN`, `LEFT`, `RIGHT`), `TAB` y atajos de guardado (`F10`).
- **Detección Automática de Dispositivos:** Identificación automática del hardware Arduino al conectar el cable OTG.
- **Configuración de Baud Rate Flexible:** Selección dinámica de velocidad de transmisión (9600, 19200, 38400, 57600, 115200 bps).
- **Consola de Diagnóstico Serie:** Visualización de mensajes de estado y datos bidireccionales en tiempo real.
- **Sin Dependencias de Nube ni Bloatware:** Aplicación 100% offline, ligera y respetuosa con los recursos.

---

## 🏛️ Arquitectura del Sistema

```mermaid
flowchart LR
    subgraph Android["📱 Dispositivo Android"]
        UI["🖥️ UI Teclado BIOS\n(Activities & Views)"]
        Manager["⚡ UsbSerialManager\n(usb-serial-for-android)"]
        UI --> Manager
    end

    subgraph Interface["🔌 Conexión Física"]
        OTG["Cable / Adaptador USB-OTG"]
        Manager <--> OTG
    end

    subgraph Arduino["🎛️ Placa Arduino"]
        MCU["Microcontrolador\n(ATmega328P / ATmega32U4)"]
        OTG <--> MCU
    end

    subgraph Target["💻 Equipo Destino"]
        PC["PC / Servidor / BIOS / UEFI"]
        MCU -- "USB HID / PS/2" --> PC
    end
```

---

## 🔌 Cómo Construir el Circuito Casero (Hardware DIY)

Para que un **Arduino Uno o Nano** (con microcontrolador ATmega328P a 5V) pueda emular un Teclado y Ratón USB a 3.3V directamente conectado al puerto USB de una computadora, se utiliza la técnica de emulación física **V-USB**. 

Puedes armar este circuito fácilmente en una protoboard o placa perforada utilizando componentes muy económicos y un cable USB reciclado.

---

### 🧰 Lista de Materiales Necesarios

| Componente | Cantidad | Especificaciones / Notas |
|---|:---:|---|
| **Placa Arduino** | 1 | Arduino Uno R3 o Arduino Nano (ATmega328P con chip CH340 o FTDI). |
| **Cable USB "Picado"** | 1 | Cable USB estándar (Tipo-A macho) cortado, pelando sus 4 hilos internos. |
| **Resistencias en serie** | 2 | **45 Ω a 68 Ω** (valores ideales: 68 Ω, 47 Ω o 56 Ω a 1/4W) para las líneas de datos. |
| **Resistencia Pull-up** | 1 | **1.5 kΩ** (1/4W) obligatoria para el estándar USB Low-Speed en D-. |
| **Diodos de limitación** | 2 | **Diodos Zener de 3.6V** (o 3.3V, ej. BZX55C3V6, 1N5227B) o diodos de cristal de alta velocidad (1N4148 / Schottky). |
| **Adaptador USB-OTG** | 1 | Para conectar el puerto USB del Arduino al teléfono móvil Android. |

---

### 🎨 Código de Colores Estándar del Cable USB Picado

Al cortar y pelar el cable USB que irá hacia la PC, encontrarás 4 hilos:

```
[CONECTOR USB TIPO-A (HACIA PC)]
  ├── ROJO   : +5V VBUS  ───────> ⚠️ ¡NO CONECTAR! Cortar y aislar con cinta térmica/aislante.
  ├── BLANCO : D- (Datos -) ────> Conectar a Pin D4 (a través de resistencia de 68Ω) + Pull-up 1.5k a 3.3V.
  ├── VERDE  : D+ (Datos +) ────> Conectar a Pin D2 (a través de resistencia de 68Ω).
  └── NEGRO  : GND (Masa)   ────> Conectar al pin GND del Arduino.
```

> [!CAUTION]
> **¡PELIGRO DE CHOQUE ELÉCTRICO CON EL CABLE ROJO (+5V)!**
> El Arduino ya recibe alimentación limpia de 5V desde tu teléfono móvil mediante el cable USB-OTG. **NUNCA conectes el cable rojo (+5V de la PC) al Arduino**. Si los 5V de la PC chocan con los 5V del teléfono, se producirán caídas de tensión, reinicios continuos del microcontrolador o disparos de la protección térmica del puerto OTG del móvil.

---

### 📐 Diagrama Esquemático del Circuito

```
                         ARDUINO UNO / NANO (ATmega328P)
                        +-------------------------------+
                        |                               |
(Teléfono Android) <==> | Puerto USB-B / Mini-USB (OTG) | (Alimentación 5V + Datos Serie)
                        |                               |
                        |                    Pin 3.3V   | ────+
                        |                               |     |
                        |                               |    [1.5 kΩ] Resistencia Pull-Up
                        |                               |     |
CABLE BLANCO (D-) ──────+───────[ 68 Ω ]──────── Pin D4 | <───+
(USB PC)                |                               |
                       === Diodo Zener 3.6V (Cátodo a D4, Ánodo a GND)
                       GND                              |
                                                        |
CABLE VERDE (D+)  ──────────────[ 68 Ω ]──────── Pin D2 | (Interrupción de hardware INT0)
(USB PC)                |                               |
                       === Diodo Zener 3.6V (Cátodo a D2, Ánodo a GND)
                       GND                              |
                                                        |
CABLE NEGRO (GND) ────────────────────────────── Pin GND|
(USB PC)                                                |
                        +-------------------------------+
```

---

### 🔬 Opciones para Limitar el Voltaje a 3.3V / 3.6V

El microcontrolador ATmega328P trabaja a **5V**, pero el bus USB requiere que las líneas de datos operen entre **2.8V y 3.6V**. Superar este voltaje puede hacer que el chipset de la laptop desconecte el puerto por protección. Puedes usar cualquiera de estas dos opciones:

#### Opción A (Recomendada): Diodos Zener de 3.6V (o 3.3V) a GND
* Coloca el **cátodo** (el lado con la franja negra o marcada) conectado a la línea de datos (D2 o D4).
* Coloca el **ánodo** conectado a la línea GND del Arduino.
* *Función:* Recorta cualquier pico superior a 3.6V hacia masa de manera limpia y segura.

#### Opción B (Alternativa): Diodos de Cristal de Alta Velocidad (1N4148 / Schottky)
* Si no dispones de diodos zener de 3.6V en tu taller, puedes usar diodos de conmutación rápida de silicio/cristal (como el **1N4148**) o diodos Schottky de pequeña señal colocados en serie o en configuración de fijación (clamping) hacia la línea de 3.3V del Arduino:
  * Ánodo a la línea de datos (D2 / D4).
  * Cátodo conectado al pin de **3.3V** del Arduino.
  * *Función:* Cuando la tensión en la línea supera 3.3V + 0.3V (aprox. 3.6V), el diodo conduce y drena el exceso de voltaje al riel de 3.3V.

---

### 📌 Tabla Resumen de Conexiones Pin a Pin

| Hilo del Cable USB (PC) | Componentes Intermedios | Pin en Arduino Uno / Nano | Propósito |
|---|---|---|---|
| **Verde (D+)** | Resistencia de **45 Ω - 68 Ω** en serie + Zener 3.6V a GND | **Pin D2** | Señal D+ (Dispara la interrupción externa `INT0` de V-USB). |
| **Blanco (D-)** | Resistencia de **45 Ω - 68 Ω** en serie + Zener 3.6V a GND | **Pin D4** | Señal D- (Línea de datos y reinicio eléctrico de bus SE0). |
| **Blanco (D-)** | Resistencia Pull-up de **1.5 kΩ** conectada al riel | **Pin 3.3V** *(o Pin D5)* | **Obligatorio:** Notifica al host que es un dispositivo *Low-Speed*. |
| **Negro (GND)** | Conexión directa | **Pin GND** | Referencia de masa común obligatoria entre PC y Arduino. |
| **Rojo (+5V)** | **NINGUNO (CORTAR/AISLAR)** | **NO CONECTAR** | Evita cortocircuitos con la alimentación del teléfono. |

---

### 🧪 Test Eléctrico de Pines con la Aplicación

Una vez ensamblado el circuito, antes de conectar la PC puedes comprobar que las resistencias y cables estén perfectos:
1. Conecta el Arduino al teléfono y abre la app **Teclado BIOS Arduino**.
2. Presiona el botón **`#TEST PINES`** (o envía `t` en la consola).
3. El microcontrolador medirá el voltaje de las líneas en reposo y responderá:
   * **`ESTADO ELECTRICO: CORRECTO`** $\to$ Cable blanco en alto (~3.3V) y verde en bajo (0V). ¡Listo para conectar a la PC!
   * **`ERROR: CABLES INVERTIDOS`** $\to$ El cable verde y blanco están al revés (intercambia D2 y D4).
   * **`ERROR: FALTA RESISTENCIA PULL-UP 1.5K`** $\to$ La línea D- está en 0V. Revisa la resistencia hacia 3.3V.


---

## 📁 Estructura del Proyecto

```
Teclado-BIOS-Arduino/
├── .gitignore
├── LICENSE
├── README.md
├── build.gradle               # Build script raíz con redirección a /tmp
├── gradle.properties          # Memoria JVM y variables de entorno
├── local.properties           # Ruta al SDK de Android
├── settings.gradle            # Configuración de repositorios y módulos
├── setup-sdk.sh               # Script de aprovisionamiento automatizado del SDK
├── app/
│   ├── build.gradle           # Configuración del módulo de aplicación
│   ├── proguard-rules.pro     # Reglas de ofuscación y optimización
│   └── src/
│       └── main/
│           ├── AndroidManifest.xml
│           ├── java/com/diamon/tecladobios/
│           └── res/
└── gradle/
    ├── libs.versions.toml     # Catálogo de versiones centralizado
    └── wrapper/
        ├── gradle-wrapper.jar
        └── gradle-wrapper.properties
```

---

## ⚙️ Requisitos Previos

- **Sistema Operativo:** Linux (Ubuntu 20.04+, Debian, etc.), macOS o Windows (WSL2).
- **JDK:** Java Development Kit (OpenJDK 17 o 21 recomendado).
- **Herramientas de Línea de Comandos:** `curl`, `unzip`, `bash`.
- **Dispositivo:** Teléfono o tablet Android con soporte para USB Host / OTG (Android 5.0 Lollipop / API 21 o superior).
- **Hardware Requerido:**
  - Adaptador o cable USB-OTG (Type-C a Type-A hembra o micro-USB a Type-A).
  - Placa Arduino (Uno, Nano, Pro Micro o Leonardo).
  - Cable USB correspondiente para conectar el Arduino.

---

## 🚀 Instalación y Configuración del SDK

El proyecto incluye el script automatizado [`setup-sdk.sh`](setup-sdk.sh) que descarga las herramientas oficiales de línea de comandos de Android, instala la plataforma `android-34` y las `build-tools 34.0.0` sin necesidad de NDK ni CMake pesados, configurando todo en `/tmp/android-sdk`.

Para ejecutarlo:

```bash
chmod +x setup-sdk.sh
./setup-sdk.sh
```

El script se encargará de:
1. Descargar `cmdline-tools` oficial de Google en `/tmp/android-sdk`.
2. Aceptar automáticamente las licencias de Android.
3. Instalar `platform-tools`, `platforms;android-34` y `build-tools;34.0.0`.
4. Generar el archivo `local.properties` con `sdk.dir=/tmp/android-sdk`.

---

## 🔨 Compilación y Construcción

Para compilar la aplicación en modo Debug:

```bash
./gradlew assembleDebug
```

Para ejecutar pruebas unitarias:

```bash
./gradlew test
```

Para limpiar la compilación:

```bash
./gradlew clean
```

El archivo APK resultante se generará en:
`/tmp/teclado-bios/app-build/outputs/apk/debug/app-debug.apk`

---

## 🎮 Guía de Uso

1. **Instalar el APK** en el dispositivo móvil Android.
2. **Cargar el firmware** correspondiente en la placa Arduino (código receptor serial a emulador HID / PS2).
3. **Conectar el Arduino al teléfono** utilizando el adaptador USB OTG.
4. Conceder el **permiso de acceso USB** cuando el sistema Android lo solicite.
5. Iniciar la aplicación **Teclado BIOS Arduino**:
   - Seleccionar el dispositivo detectado en la lista.
   - Elegir la velocidad en baudios (por defecto: `115200` o `9600`).
   - Pulsar **Conectar**.
6. Conectar la salida del Arduino a la PC y encender el equipo:
   - Pulsar `DEL` o `F2` repetidamente desde la app para ingresar a la configuración de la BIOS/UEFI.
   - Navegar con las flechas direccionales táctiles y configurar los parámetros necesarios.

---

## 💾 Optimización de Disco y Entorno

Para evitar el agotamiento de espacio en entornos con almacenamiento limitado (por ejemplo Cloud Shell o contenedores CI/CD), este repositorio aplica redirecciones estratégicas:

- **Build Output:** Redirigido a `/tmp/teclado-bios/root-build` y `/tmp/teclado-bios/app-build`.
- **Gradle User Home:** Centralizado en `/tmp/.gradle`.
- **Android SDK:** Ubicado en `/tmp/android-sdk`.

---

## 📄 Licencia

Este proyecto está bajo la Licencia Apache 2.0. Consulta el archivo [LICENSE](LICENSE) para más detalles.

Copyright (c) 2026 Daniel Diamon.
