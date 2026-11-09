#!/usr/bin/env bash
#
# Compila arduino_bios_keyboard_v2.ino y actualiza el .hex en la raíz del
# repositorio y en app/src/main/assets/firmware/.
#
# Uso:
#   ./firmware/build.sh
#
# Requisitos:
#   - arduino-cli en $HOME/.local/bin/arduino-cli (o definir ARDUINO_CLI)
#   - core arduino:avr instalado:
#       arduino-cli core install arduino:avr
#
# La librería UsbKeyboard (V-USB compuesta teclado + ratón) vive en
# firmware/UsbKeyboardBIOS y se pasa con --libraries.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARDUINO_CLI="${ARDUINO_CLI:-$HOME/.local/bin/arduino-cli}"
SKETCH_NAME="arduino_bios_keyboard_v2"
FQBN="${FQBN:-arduino:avr:uno}"
BUILD_DIR="${BUILD_DIR:-/tmp/teclado-fw-build}"

if [[ ! -x "${ARDUINO_CLI}" ]]; then
  echo "ERROR: no se encontró arduino-cli en ${ARDUINO_CLI}" >&2
  echo "       instálalo o exporta ARDUINO_CLI=/ruta/a/arduino-cli" >&2
  exit 1
fi

echo "==> Preparando sketch en ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/${SKETCH_NAME}"
cp "${ROOT}/${SKETCH_NAME}.ino" "${BUILD_DIR}/${SKETCH_NAME}/"

echo "==> Compilando con ${FQBN}"
TMPDIR="${TMPDIR:-/tmp}" "${ARDUINO_CLI}" compile \
  --fqbn "${FQBN}" \
  --libraries "${ROOT}/firmware" \
  --build-path "${BUILD_DIR}/build" \
  --output-dir "${BUILD_DIR}/out" \
  "${BUILD_DIR}/${SKETCH_NAME}"

echo "==> Copiando ${SKETCH_NAME}.hex"
cp "${BUILD_DIR}/out/${SKETCH_NAME}.ino.hex" "${ROOT}/${SKETCH_NAME}.hex"
cp "${ROOT}/${SKETCH_NAME}.ino" "${ROOT}/app/src/main/assets/firmware/"
cp "${ROOT}/${SKETCH_NAME}.hex" "${ROOT}/app/src/main/assets/firmware/"

echo "==> Listo"
md5sum "${ROOT}/${SKETCH_NAME}.hex" "${ROOT}/app/src/main/assets/firmware/${SKETCH_NAME}.hex"
