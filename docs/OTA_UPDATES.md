# Remote updates (OTA)

Once the lamps are at their new homes, new firmware goes over the air: an update takes about a minute and works wherever the lamp is. You need the firmware hosting from [server/OTA_HOSTING.md](../server/OTA_HOSTING.md), SSH access to that server, and the Mosquitto clients on your computer.

**Rule: try every new version on lamp A first.**

## How it works

1. `tools/update_lamp.sh` checks that the `.bin` file carries the tag of the right lamp (`LAMPADA-ID:A` or `LAMPADA-ID:B`, written into the firmware at compile time). With the wrong lamp it stops.
2. It copies the file to the server as `lampada_A.bin` or `lampada_B.bin`, in the folder published by Tailscale Funnel.
3. It sends `update` to `<base><ID>/cmd`.
4. The lamp sets its retained status to `aggiornamento in corso` ("update in progress"), closes the MQTT connection to free memory for a second TLS connection, and downloads `FW_BASE + "lampada_<ID>.bin"`. The address is compiled into the firmware: the MQTT command cannot point the lamp anywhere else. A light blue bar fills the ring as the download progresses.
5. If the download works, the lamp restarts on the new firmware.
6. **Rollback**: the new firmware stays "on trial" until it connects to the broker. If it crashes or restarts before that, the bootloader goes back to the previous version by itself. Once connected, the firmware marks itself valid.
7. If the download fails, the lamp tries **3 times** (30 s HTTP timeout, 2 s pause between attempts). Then it flashes red three times, stays on the old version, reconnects and publishes the reason on `<base><ID>/log`.
8. The script follows `/stato` and `/log` and stops when, after `aggiornamento in corso`, the lamp is `online` again (or after 7 minutes).

## Running an update

1. Open `firmware/twin_lamps/twin_lamps.ino` in the Arduino IDE (`secrets.h` is already next to it and does not change) and bump `FW_VERSION`.
2. Set `#define LAMP_ID 'A'` and save.
3. **Sketch → Export Compiled Binary.**
4. The first time, copy `tools/.env.example` to `tools/.env` and fill it in.
5. Run:

   ```bash
   ./tools/update_lamp.sh A
   ```

   The script finds the most recent exported `twin_lamps.ino.bin` in your home folder (or pass the path as the second argument). Answer `y`, then type the SSH password if your server asks for one, then the MQTT password.
6. Watch the ring fill with light blue, then restart. The terminal should end with `online v<new version>`.
7. Repeat with `LAMP_ID 'B'` and `./tools/update_lamp.sh B`, then set `LAMP_ID` back to `'A'` and save.
8. **Clean up**: the `.bin` contains the broker password and the firmware address, so delete it from the server and remove the local `build` folder:

   ```bash
   ssh user@your-server rm /srv/lamps-fw/lampada_A.bin /srv/lamps-fw/lampada_B.bin
   ```

## Requirements

- **Partition scheme**: Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS). The firmware takes about 1.23 MB, 62% of the app partition. With the Default scheme it would take about 92% and future updates would not fit. The partition scheme can only be changed over USB, so set it on both lamps before they leave.
- **Signal**: from −70 dBm up the Wi-Fi is fine. One update was aborted at −86 dBm (while Funnel was also not answering), and the lamp simply stayed on its old version; at −78 dBm updates work in about a minute. The `rssi` value is in the status message.

## Error messages

Errors are published on `<base><ID>/log`, in Italian, as `aggiornamento fallito: tentativo 1: <error> (scaricati X/Y KB, rssi=-70); tentativo 2: ...` ("update failed: attempt 1: <error> (downloaded X/Y KB, ...)").

| Error | Meaning |
| --- | --- |
| `scrittura flash` | Flash write failed |
| `cancellazione flash` | Flash erase failed |
| `lettura flash` | Flash read failed |
| `spazio insufficiente` | Not enough space (check the partition scheme) |
| `dimensione non valida` | Invalid size |
| `download interrotto o troppo lento` | Download interrupted or too slow |
| `MD5 errato` | Wrong MD5 checksum |
| `file non valido (non è un firmware)` | Invalid file: not a firmware (wrong URL, or an error page instead of the `.bin`) |
| `attivazione` | Could not activate the new partition |
| `nessuna partizione OTA` | No OTA partition (wrong partition scheme) |
| `argomento non valido` | Invalid argument |
| `annullato` | Aborted |
| `decifratura`, `firma`, `SHA256`, `SHA512` | Decryption, signature or hash errors |
| `nessun aggiornamento` | The server answered "no update" (HTTP 304) |
| `codice N` | Any other error code |

Other messages come straight from the HTTP client (for example a 404 when the file is not on the server).
