// secrets.example.h — template for secrets.h.
// Copy this file to secrets.h (same folder) and put your own values in it.
// secrets.h is in .gitignore: it stays on your PC, never commit or share it.
// When a new version of twin_lamps.ino comes out, you do not need to touch secrets.h.

// MQTT broker (EMQX Serverless: Overview page of your deployment)
#define SECRET_MQTT_HOST  "your-deployment.emqxsl.com"
#define SECRET_MQTT_USER  "your-mqtt-user"
#define SECRET_MQTT_PASS  "your-mqtt-password"

// "Private channel" of the two lamps: pick something hard to guess. Must end with "/".
#define SECRET_TOPIC_BASE "lampade/your-secret-channel/"

// Password of the temporary Wi-Fi network used for the first setup (at least 8 characters)
#define SECRET_AP_PASS    "your-ap-password"

// Folder published with Tailscale Funnel that serves lampada_A.bin / lampada_B.bin.
// Must end with "/". See server/OTA_HOSTING.md.
#define SECRET_FW_BASE    "https://your-server.your-tailnet.ts.net:10000/fw-xxxxxxxxxxxxxxxx/"
