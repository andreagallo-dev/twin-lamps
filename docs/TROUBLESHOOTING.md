# Troubleshooting

Commands like `ping`, `touch` or `luce` are MQTT commands: see [MQTT_COMMANDS.md](MQTT_COMMANDS.md) for how to send them.

## The lamp

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| Light blue ring | The saved Wi-Fi is not found (first boot, after an erase, or a new home) | Join the lamp's own network from a phone ("Lampada di Andre" or "❤Lampada di Cate❤"), open `192.168.4.1` if the page does not appear by itself, then Configure WiFi → pick a **2.4 GHz** network → password → Save. You have 5 minutes, then the lamp restarts and opens the network again. |
| Solid red dot on one LED | Wi-Fi works but the broker is unreachable | Wait: it retries every 5 seconds. Check the EMQX console (is the deployment running?). Restart the lamp. |
| Status `offline` | The lamp is off or has no network | Check that it is plugged in; send `ping` when it is back. |
| Ghost touches (flashes nobody asked for) | Electrical noise on the touch input | Leave the lamp untouched for 10 minutes, then send `touch`: if the minimum drops below the threshold, lower `touch_ratio` (for example `touch_ratio 0.55`). |
| A touch is not detected | Threshold too strict, or the lamp was touched while starting (it calibrates at boot) | Restart it without touching it, then raise `touch_ratio` if needed. |
| Too bright or too dark in the evening | Photoresistor thresholds | Send `luce` in the light you care about, then adjust with `ldr_buio` and `ldr_luce`. Put the final values in the code. |
| The lamp restarts when the LEDs are bright | Powered from a PC USB port (brownout) | Use the 5 V 1 A charger. |
| An unseen message LED stays on | Working as intended: a "thinking of you" ended and nobody touched the lamp | Touch the lamp. |
| Testing with a phone hotspot, the phone cannot open the setup page | A phone cannot be a hotspot and join the lamp's setup network at the same time | Join the lamp's network from a second device (a laptop) and pick the phone's hotspot there. |

## Remote updates

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| `bad user name or password` in the terminal | `MQTT_PASS` is empty in this terminal | Run `read -r -s MQTT_PASS` again. |
| `Could not resolve hostname your-server` | Tailscale is off on the laptop | `sudo tailscale up` |
| `ERROR: this file is NOT the firmware of lamp B` | `LAMP_ID` in the exported binary does not match | Set the right `LAMP_ID`, save, export again. |
| `aggiornamento fallito: ...` on `/log` | Weak signal, or Funnel momentarily down | Read the reason ([OTA_UPDATES.md](OTA_UPDATES.md#error-messages)). Open the firmware URL from a phone with Wi-Fi and Tailscale off; run `tailscale funnel status` on the server; try again. |
| The script never sees `online` again | The new firmware does not reach the broker | Wait: if it crashes before connecting, the rollback brings back the old version and the lamp comes back online by itself. |

## USB, Arduino IDE and Serial Monitor

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| Upload stuck on `Connecting...` | The board is not entering download mode | Hold the BOOT button until the percentages start. |
| Unreadable characters in the Serial Monitor after an upload | Upload speed 921600 left the monitor at the wrong speed | Set Upload Speed to 115200 and the Serial Monitor to 115200 baud. |
| Serial Monitor frozen or unreadable | The monitor is still attached, or the speed is wrong | Unplug the cable, close the monitor (or `pkill -f serial-monitor`), set 115200, plug in again and press EN. |
| esptool says `Detected crystal freq 15.32 MHz` | False alarm | Ignore it: the serial port works at 115200. |
| The lamp should forget the saved Wi-Fi | — | Upload once with Tools → Erase All Flash Before Sketch Upload: Enabled, then set it back to Disabled. |
| The sketch does not fit, or OTA reports no space | Wrong partition scheme | Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS), set over USB. |
| `/dev/ttyUSB0` permission denied (Linux) | Your user cannot access serial ports | Add your user to the `dialout` group and log in again. |

## Broker

| Symptom | Likely cause | What to do |
| --- | --- | --- |
| The deployment was stopped | 30 days without any connected client | Start it again from the EMQX console and run the keepalive container ([ARCHITECTURE.md](ARCHITECTURE.md#broker-and-keepalive)). |
| Lost the laptop with `secrets.h` | — | Rebuild it: the MQTT password from the EMQX console (Access Control → Authentication), the firmware address from `tailscale funnel status` on the server. |
