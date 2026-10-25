#!/usr/bin/env bash
# Updates one lamp over the air (OTA).
# Usage:  ./update_lamp.sh A        (or B)
#         ./update_lamp.sh B /path/to/twin_lamps.ino.bin
#
# Before: Arduino IDE -> right LAMP_ID -> bump FW_VERSION -> Sketch -> Export Compiled Binary.
# Private settings come from .env next to this script (copy .env.example to .env).
# Needs ssh/scp access to the server and mosquitto-clients (mosquitto_pub, mosquitto_sub).
#
# The lamps speak Italian on MQTT: this script looks for the status "aggiornamento in corso"
# ("update in progress") and "online", and for the LAMPADA-ID tag inside the .bin.
set -euo pipefail

# ---- private settings, read from .env ----
ENV_FILE="$(dirname "$0")/.env"
[[ -f "$ENV_FILE" ]] || { echo "Missing $ENV_FILE: copy .env.example to .env and fill it in."; exit 1; }
# shellcheck source=.env.example
source "$ENV_FILE"
: "${SERVER:?SERVER missing in .env}" "${FW_DIR:?FW_DIR missing in .env}"
: "${MQTT_HOST:?MQTT_HOST missing in .env}" "${MQTT_USER:?MQTT_USER missing in .env}"
: "${TOPIC_BASE:?TOPIC_BASE missing in .env}"
# -------------------------------------------

ID="${1:-}"
if [[ "$ID" != "A" && "$ID" != "B" ]]; then
  echo "Usage: $0 A|B [file.bin]"; exit 1
fi

BIN="${2:-}"
if [[ -z "$BIN" ]]; then   # the most recent exported .bin in your home folder
  BIN=$(find "$HOME" -maxdepth 7 -path '*build/esp32.esp32.esp32/twin_lamps.ino.bin' \
          -printf '%T@ %p\n' 2>/dev/null | sort -n | tail -1 | cut -d' ' -f2-)
fi
[[ -f "$BIN" ]] || { echo ".bin file not found: use Sketch -> Export Compiled Binary"; exit 1; }

echo "File:  $BIN"
echo "Date:  $(date -r "$BIN" '+%Y-%m-%d %H:%M')"

# Safety check: the firmware must be the one for this lamp
if ! grep -a -q "LAMPADA-ID:$ID" "$BIN"; then
  echo "ERROR: this file is NOT the firmware of lamp $ID (check LAMP_ID)."; exit 1
fi
echo "OK: firmware of lamp $ID"

read -r -p "Update lamp $ID now? [y/N] " ok
[[ "$ok" == "y" || "$ok" == "Y" ]] || exit 0

echo "Copying to the server..."
scp -q "$BIN" "$SERVER:$FW_DIR/lampada_$ID.bin"

read -r -s -p "MQTT password: " MQTT_PASS; echo
M=(-h "$MQTT_HOST" -p 8883 -u "$MQTT_USER" -P "$MQTT_PASS" --capath /etc/ssl/certs)

echo "Current status: $(mosquitto_sub "${M[@]}" -t "${TOPIC_BASE}$ID/stato" -C 1 -W 5 2>/dev/null || echo 'not available')"
mosquitto_pub "${M[@]}" -t "${TOPIC_BASE}$ID/cmd" -m update
echo "Command sent. Waiting for the lamp to restart (up to 7 minutes)..."

# Shows the status and error messages: first "aggiornamento in corso", then "online" again
timeout 420 mosquitto_sub "${M[@]}" -t "${TOPIC_BASE}$ID/stato" -t "${TOPIC_BASE}$ID/log" -v 2>/dev/null \
  | { seen=0
      while read -r line; do
        echo "  $line"
        [[ "$line" == *"aggiornamento in corso"* ]] && seen=1
        if [[ $seen == 1 && "$line" == *"/stato online"* ]]; then
          pkill -P $$ -x timeout 2>/dev/null || true; break
        fi
      done; } || true

echo "Done. If you see 'online v...' with the new version, the update worked."
echo "Delete the file from the server (it contains the broker password): ssh $SERVER rm $FW_DIR/lampada_$ID.bin"
