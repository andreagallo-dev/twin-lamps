# MQTT topics and commands

The lamps speak **Italian** on MQTT: command names, topics, status strings and replies are exactly what the deployed lamps send and expect, so they are kept as they are. This page explains each of them in English.

In the examples, `<base>` is the topic base set in `secrets.h` (`SECRET_TOPIC_BASE`, for example `lampade/your-secret-channel/`) and `<ID>` is `A` or `B`.

## Topics

| Topic | Who publishes | Content |
| --- | --- | --- |
| `<base>A`, `<base>B` | Each lamp on its own ID; it listens to the other one | Gestures: `tap`, `heart_on`, `heart_off`, `goodnight` |
| `<base><ID>/cmd` | You | Commands for that lamp (below) |
| `<base><ID>/stato` ("status") | The lamp, **retained** | `online v2.6 rssi=-62` when connected, `aggiornamento in corso` ("update in progress") during an update, `offline` (last will, set by the broker when the lamp drops off) |
| `<base><ID>/log` | The lamp | Replies to commands and update errors |

The MQTT client ID is `lampada-<ID>-<chip id>`.

## Messages between the lamps

| Message | Sent when | The other lamp |
| --- | --- | --- |
| `tap` | Short tap, as soon as the finger is lifted | Breathes in the sender's color for 10 minutes. If it also sent a `tap` in the last 15 s, both play "together". |
| `heart_on` | The finger stays on the hook for more than 0.9 s | Beats like a heart until `heart_off` (or for 90 s at most, in case `heart_off` gets lost) |
| `heart_off` | The finger is lifted after a heartbeat | Stops the heartbeat and starts breathing |
| `goodnight` | Double tap (second tap within 0.4 s) | Plays the 60-second sunset and turns off |

A double tap sends `tap` and then `goodnight`: the sunset replaces the breathing that just started.

## Commands

Publish them on `<base><ID>/cmd`. Values changed with a command last **until the next restart**: once you like them, copy them into the firmware and update it.

| Command | Meaning | Reply on `/log` |
| --- | --- | --- |
| `update` | Download and install the new firmware (see [OTA_UPDATES.md](OTA_UPDATES.md)) | Status changes on `/stato`; errors on `/log` |
| `ping` | Publish the status again | — (new `/stato`) |
| `riavvia` | "Restart": reboot the lamp | — |
| `prova <animation>` | "Test": play an animation now, without waiting for the date (list below) | — |
| `colore <name> R G B` | "Color": change one palette color and show it for 60 s | The new line ready for the code, e.g. `{"giallo", {255, 115, 5}},`, or `uso: colore <nome> R G B` ("usage") if the command is wrong |
| `mostra <name>` | "Show": a palette color, solid, for 60 s | — |
| `colori` | "Colors": the whole palette, ready to paste into `PAL[]` | One line per color |
| `livello_rosso 0.8` | "Red level": brightness of the red of the 30th (0.05–1); shows it for 60 s | `RED_LEVEL = 0.80` |
| `livello_respiro 0.45` | "Breathing level": brightness of the "thinking of you" glow (0.1–0.85) | `GLOW_LEVEL = 0.45` |
| `luce` | "Light": photoresistor reading | `luce=1242 ambient=1.00 (buio sotto 15, piena luce sopra 600)`: reading, brightness factor, then "dark below 15, full light above 600" |
| `ldr_buio N` | "LDR dark": reading below which the room counts as dark | `LDR_DARK = N` |
| `ldr_luce N` | "LDR light": reading above which the lamp is at full brightness | `LDR_BRIGHT = N` |
| `touch` | Touch sensor readings | `touch: riposo X, minimo da ultimo controllo Y, soglia Z (touch_ratio R)`: value at rest, lowest value since the last `touch` command, current threshold |
| `touch_ratio 0.65` | Touch sensitivity (0.3–0.95): the lamp counts a touch when the reading drops below rest × ratio. Lower means a firmer touch is needed. | `touch_ratio = 0.65` |

### Animations for `prova`

| Name | Meaning | What it plays |
| --- | --- | --- |
| `anniversario` | Anniversary | 90 s: breathing burgundy base with yellow, gold, orange, green and champagne fireworks |
| `mese` | Month | The 21:00 animation of the 30th: the red makes room for two comets (green and yellow) running in opposite directions for 25 s |
| `rosso` | Red | The all-day red of the 30th, for 60 s |
| `compleanno_cate` | Cate's birthday | 60 s: yellow, orange and gold party |
| `compleanno_andre` | Andre's birthday | 60 s: green, lime and gold party |
| `san_valentino` | Valentine's Day | 45 s: burgundy heartbeat with pink sparks |
| `natale` | Christmas | 60 s: mostly red, with fir green, gold and white |
| `capodanno` | New Year | 60 s: fireworks, mostly gold, red and champagne |
| `buonanotte` | Goodnight | The 60-second sunset |
| `penso` | "Thinking of you" | Lights the single LED of an unseen message (until you touch the lamp) |

### Palette names

The color names are used by `colore` and `mostra`.

| Name | Meaning | R, G, B | Used for |
| --- | --- | --- | --- |
| `verde` | green | 55, 200, 0 | Lamp A (Andre) |
| `giallo` | yellow | 255, 115, 5 | Lamp B (Cate) |
| `verde2` | lime | 100, 250, 0 | Green's companion in the breathing wave and the parties |
| `giallo2` | orange | 255, 45, 0 | Yellow's companion |
| `rosso` | red | 255, 0, 0 | The 30th of the month, anniversary, Valentine's Day, Christmas |
| `abete` | fir green | 5, 110, 10 | Christmas |
| `oro` | gold | 255, 140, 10 | Warm sparks |
| `arancio` | orange | 255, 70, 0 | Sunset, New Year |
| `brace` | ember | 130, 5, 0 | End of the sunset |
| `rosa` | pink | 255, 40, 70 | Valentine's Day |
| `champagne` | champagne | 255, 140, 30 | Light sparks |
| `bianco` | warm white | 255, 170, 80 | Christmas, New Year |

On the LEDs green is much stronger than red, and a few points of blue are enough to turn red into pink or green into light blue.

## From a terminal

Install the Mosquitto clients (`mosquitto-clients` on Debian and Ubuntu). In each new terminal, first read the password into a variable (it stays only in that terminal), then define a small helper:

```bash
read -r -s MQTT_PASS
```

```bash
lamp() { mosquitto_pub -h your-deployment.emqxsl.com -p 8883 -u your-mqtt-user -P "$MQTT_PASS" --capath /etc/ssl/certs -t "lampade/your-secret-channel/$1/cmd" -m "$2"; }
```

Then, for example:

```bash
lamp A "prova natale"
```

To watch status, gestures and replies of both lamps:

```bash
mosquitto_sub -h your-deployment.emqxsl.com -p 8883 -u your-mqtt-user -P "$MQTT_PASS" --capath /etc/ssl/certs -t 'lampade/your-secret-channel/#' -v
```

To pretend to be lamp B and send a gesture to lamp A, publish on B's topic:

```bash
mosquitto_pub -h your-deployment.emqxsl.com -p 8883 -u your-mqtt-user -P "$MQTT_PASS" --capath /etc/ssl/certs -t "lampade/your-secret-channel/B" -m tap
```
