/*
  TWIN LAMPS v2 — firmware for ESP32 (classic, WROOM-32)

  Gestures:
    - short tap        -> the other lamp lights up in your color and "breathes" for 10 minutes
    - double tap       -> "goodnight": the other lamp does a slow sunset and turns off
    - touch and hold   -> the other lamp pulses like a heartbeat for as long as your finger stays on
    - both touch within 15 s -> "together" animation, with the two colors rotating

  Also:
    - unseen "thinking of you": if the breathing ends without you touching the lamp,
      one LED in the other person's color stays on until you touch yours
    - special dates (time from the internet, Italian time zone):
        30/06 anniversary, 01/03 Cate's birthday, 01/04 Andre's birthday (example dates),
        14/02 Valentine's Day, 25/12 Christmas, 01/01 New Year at midnight
    - on the 30th of every month (in February the last day): solid red all day,
      from midnight to midnight, with a dedicated animation at 21:00
    - over-the-air update (OTA) and lamp status via MQTT:
        <TOPIC_BASE><ID>/cmd   commands: update | ping | riavvia | prova <animation>
                               colore <name> R G B | mostra <name> | livello_rosso 0.8 | livello_respiro 0.45 | colori
                               luce | ldr_buio N | ldr_luce N | touch | touch_ratio 0.65
                               (replies arrive on <TOPIC_BASE><ID>/log)
        <TOPIC_BASE><ID>/stato "online vX" or "offline" (retained)
      animations you can test: anniversario, mese, rosso, compleanno_cate,
      compleanno_andre, san_valentino, natale, capodanno, buonanotte, penso
      The protocol (commands, topics, status and replies) is in Italian, exactly as on the
      deployed lamps: docs/MQTT_COMMANDS.md explains every command in English.

  Libraries (Arduino IDE -> Library Manager):
    - WiFiManager (tzapu)
    - PubSubClient (Nick O'Leary)
    - Adafruit NeoPixel
  Board: "ESP32 Dev Module" (Espressif ESP32 core)
  Partition Scheme: "Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)"

  Wiring:
    WS2812B ring    DIN -> GPIO13 (with a 330 ohm resistor in series)
                    5V  -> VIN (5V from USB)
                    GND -> GND
                    100 uF capacitor between 5V and GND ("-" stripe to GND)
    Touch:          wire from GPIO4 (T0) to the metal hook of the box
    Photoresistor:  3V3 -> photoresistor -> GPIO34 -> 10 kohm resistor -> GND
                    (more light = higher value). It must not see the light of the lamp.
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <HTTPUpdate.h>
#include <time.h>
#include <esp_ota_ops.h>
#include "secrets.h"   // broker, passwords and firmware address: separate file, stays on your PC
                       // (copy secrets.example.h to secrets.h and fill it in)

#define FW_VERSION "2.6"

// ======================= CONFIGURATION =======================
// Change ONLY this line between the two lamps: 'A' = yours, 'B' = the other one
#define LAMP_ID 'A'

// MQTT broker (EMQX Cloud Serverless)
const char*    MQTT_HOST = SECRET_MQTT_HOST;
const uint16_t MQTT_PORT = 8883;
const char*    MQTT_USER = SECRET_MQTT_USER;
const char*    MQTT_PASS = SECRET_MQTT_PASS;

// "Private channel" of the two lamps
const char* TOPIC_BASE = SECRET_TOPIC_BASE;

// Temporary Wi-Fi network for the first setup (password at least 8 characters).
// The names mean "Andre's lamp" and "Cate's lamp".
const char* AP_NAME_A = "Lampada di Andre";
const char* AP_NAME_B = "\xE2\x9D\xA4Lampada di Cate\xE2\x9D\xA4";   // ❤Lampada di Cate❤
const char* AP_PASS   = SECRET_AP_PASS;

// Remote updates: folder published with Tailscale Funnel (ends with "/").
// The lamp downloads FW_BASE + "lampada_A.bin" or "lampada_B.bin".
const char* FW_BASE = SECRET_FW_BASE;

// Colors (R, G, B). You can try them live with the MQTT command "colore <name> R G B"
// and then copy the chosen values here. On the LEDs green is much stronger than red, and
// a little blue is enough to turn red into pink or green into light blue.
// The names stay in Italian because the MQTT commands use them.
struct RGB { uint8_t r, g, b; };
struct NamedColor { const char* name; RGB c; };
enum { C_GREEN, C_YELLOW, C_GREEN2, C_YELLOW2, C_RED, C_FIR, C_GOLD,
       C_ORANGE, C_EMBER, C_PINK, C_CHAMPAGNE, C_WHITE, C_N };
NamedColor PAL[C_N] = {               // colors picked live on the finished lamps
  {"verde",     { 55, 200,   0}},   // green: Andre
  {"giallo",    {255, 115,   5}},   // yellow: Cate, a sunflower at sunset
  {"verde2",    {100, 250,   0}},   // green's companion (lime): the shades that make it feel alive
  {"giallo2",   {255,  45,   0}},   // yellow's companion (orange)
  {"rosso",     {255,   0,   0}},   // red of the 30th of the month
  {"abete",     {  5, 110,  10}},   // fir: Christmas green
  {"oro",       {255, 140,  10}},   // gold: warm sparks
  {"arancio",   {255,  70,   0}},   // orange: sunset
  {"brace",     {130,   5,   0}},   // ember: end of the sunset
  {"rosa",      {255,  40,  70}},   // pink: Valentine's Day
  {"champagne", {255, 140,  30}},   // light sparks of the fireworks
  {"bianco",    {255, 170,  80}},   // white: warm white
};
float RED_LEVEL = 1.0f;                // brightness of the solid red (0-1)
float GLOW_LEVEL = 0.45f;              // brightness of "thinking of you" (0-1), about half of v2.5

#define LED_PIN     13
#define NUM_LEDS    16
#define MAX_BRIGHT  255     // 0-255: brightness limit. At this level you need the 1 A charger:
                            // from a PC USB port the board may reset (brownout)
#define TOUCH_PIN   T0      // GPIO4
#define TOUCH_RATIO 0.65f   // touched if reading < baseline * TOUCH_RATIO (tune it with "touch_ratio")
#define PRESS_MS    50      // the finger must stay at least 50 ms: filters out noise (ghost touches)
#define PENDING_LED 8       // LED used for the unseen "thinking of you"

// Automatic brightness with a photoresistor (set to 0 to disable it)
#define USE_LDR     1
#define LDR_PIN     34      // only pins 32-39 can read analog values while Wi-Fi is on
int         LDR_DARK    = 15;    // reading below which the room is "dark" (measured: dark 0-15)
int         LDR_BRIGHT  = 600;   // reading above which the room is "lit" (daylight 1700-2740)
                                 // tune them live with "ldr_buio N" and "ldr_luce N"
const float NIGHT_LEVEL = 0.25f; // relative brightness in the dark (0-1)

const uint32_t HOLD_MS       = 900;                // longer = heartbeat
const uint32_t DOUBLE_WIN    = 400;                // max time between the two taps of a double tap
const uint32_t GLOW_MS       = 10UL * 60 * 1000;   // "thinking of you" duration
const uint32_t GOODNIGHT_MS  = 60000;              // "goodnight" sunset duration
const uint32_t TOGETHER_WIN  = 15000;              // window for touching together
const uint32_t TOGETHER_MS   = 30000;              // "together" animation duration
const uint32_t HEART_TIMEOUT = 90000;              // safety net if heart_off gets lost
const uint32_t TAP_COOLDOWN  = 800;
const uint32_t RELEASE_MS    = 60;                 // release debounce
const uint32_t GLOW_STOP_MS  = 2000;               // fade-out when you touch during "thinking of you"

// Special dates: Italian time, with automatic daylight saving time
const char* TZ_ITALY = "CET-1CEST,M3.5.0,M10.5.0/3";
const uint8_t MONTH_HOUR = 21, MONTH_MIN = 0;      // time of the 30th-of-the-month animation
const uint8_t ANNIV_DAY = 30, ANNIV_MONTH = 6;     // example date: anniversary

enum Anim : uint8_t { AN_NONE, AN_ANNIV, AN_MONTH, AN_BDAY_B, AN_BDAY_A,
                      AN_VALENTINE, AN_XMAS, AN_NEWYEAR, AN_COUNT };
const uint32_t ANIM_MS[AN_COUNT] = {0, 90000, 25000, 60000, 60000, 45000, 60000, 60000};

struct SpecialDate { uint8_t day, month, hour, minute; Anim anim; };
const SpecialDate DATES[] = {
  {ANNIV_DAY, ANNIV_MONTH, 21, 0, AN_ANNIV},   // example date: anniversary
  { 1,  3, 21, 0, AN_BDAY_B},      // example date: Cate's birthday (yellow)
  { 1,  4, 21, 0, AN_BDAY_A},      // example date: Andre's birthday (green)
  {14,  2, 21, 0, AN_VALENTINE},   // Valentine's Day
  {25, 12, 10, 0, AN_XMAS},        // Christmas, in the morning
  { 1,  1,  0, 0, AN_NEWYEAR},     // New Year, at midnight
};
// ===============================================================

// Tag inside the .bin file: the update script checks it
#if LAMP_ID == 'A'
#define LAMP_TAG "LAMPADA-ID:A"
#else
#define LAMP_TAG "LAMPADA-ID:B"
#endif

const char OTHER_ID = (LAMP_ID == 'A') ? 'B' : 'A';
#define COL(i)       (PAL[i].c)
#define COLOR_A      COL(C_GREEN)
#define COLOR_B      COL(C_YELLOW)
#define RED_LOVE     COL(C_RED)
#define MY_COLOR     ((LAMP_ID == 'A') ? COL(C_GREEN)  : COL(C_YELLOW))
#define MY_COLOR2    ((LAMP_ID == 'A') ? COL(C_GREEN2) : COL(C_YELLOW2))
#define OTHER_COLOR  ((LAMP_ID == 'A') ? COL(C_YELLOW) : COL(C_GREEN))
#define OTHER_COLOR2 ((LAMP_ID == 'A') ? COL(C_YELLOW2): COL(C_GREEN2))

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WiFiClientSecure net;
PubSubClient mqtt(net);
String pubTopic, subTopic, cmdTopic, statTopic, logTopic;

// --- animation state ---
bool glowOn = false;      uint32_t glowStart = 0;     bool glowSeen = false;
bool glowStopping = false; uint32_t glowStopStart = 0;  // touch during "thinking of you": slow fade-out
bool heartRxOn = false;   uint32_t heartRxStart = 0;
bool togetherOn = false;  uint32_t togetherStart = 0;
bool feedbackOn = false;  uint32_t feedbackStart = 0;
bool gnOn = false;        uint32_t gnStart = 0;       // goodnight received
bool gnFbOn = false;      uint32_t gnFbStart = 0;     // goodnight sent (confirmation)
bool pendingOn = false;                               // unseen "thinking of you"
Anim evAnim = AN_NONE;    uint32_t evStart = 0;
bool redPreview = false;  uint32_t redPreviewStart = 0;
int  showColor = -1;      uint32_t showStart = 0;    // "mostra <color>" command
bool heartTx = false;     uint32_t heartTxStart = 0;

// --- touch state ---
bool tapSentEver = false; uint32_t lastTapSent = 0;
bool tapRxEver = false;   uint32_t lastTapRx = 0;
float touchBaseline = 0;
float touchRatio = TOUCH_RATIO;
float touchMin = 1e9;                                 // lowest reading since the last "touch" command
bool candidate = false;   uint32_t candStart = 0;     // possible touch, confirmed after PRESS_MS
bool touching = false;
uint32_t pressStart = 0, lastTouchSeen = 0;
bool tapWaiting = false;  uint32_t tapReleaseAt = 0;  // first tap done, waiting for a possible second one
bool secondPress = false;

// --- network ---
uint32_t lastMqttAttempt = 0;
bool mqttAttempted = false;
bool appMarkedValid = false;
bool otaRequested = false;
String otaError;

// --- clock ---
bool timeOk = false;
struct tm nowTm;

// --- ambient light ---
float ambient = 1.0f;        // brightness factor, NIGHT_LEVEL..1
float ldrSmooth = -1.0f;
uint32_t lastLit = 0;        // last time the lamp was showing an animation

// With rollback enabled, a new firmware stays "on trial" until it connects to the broker:
// if it gets stuck before that, on the next boot it goes back to the previous version by itself.
extern "C" bool verifyRollbackLater() { return true; }

// ======================= LIGHTS =======================
float acc[NUM_LEDS][3];
float spLvl[NUM_LEDS];  RGB spCol[NUM_LEDS];
uint32_t lastSpark = 0, lastDecay = 0;

void clr() { memset(acc, 0, sizeof(acc)); }

int wrap(int i) { return ((i % NUM_LEDS) + NUM_LEDS) % NUM_LEDS; }

void addPx(int i, RGB c, float level) {
  level = constrain(level, 0.0f, 1.0f);
  level = level * level;                       // perceptual correction
  i = wrap(i);
  acc[i][0] += c.r * level;  acc[i][1] += c.g * level;  acc[i][2] += c.b * level;
}

void putPx(int i, RGB c, float level) {
  i = wrap(i);
  acc[i][0] = acc[i][1] = acc[i][2] = 0;
  addPx(i, c, level);
}

void fillPx(RGB c, float level) { for (int i = 0; i < NUM_LEDS; i++) putPx(i, c, level); }

void flush(bool offlineDot) {
  for (int i = 0; i < NUM_LEDS; i++)
    strip.setPixelColor(i, (uint8_t)min(255.0f, acc[i][0] * ambient),
                           (uint8_t)min(255.0f, acc[i][1] * ambient),
                           (uint8_t)min(255.0f, acc[i][2] * ambient));
  if (offlineDot) strip.setPixelColor(0, 20, 0, 0);   // red dot = not connected
  strip.show();
}

float bump(float x) { return (x < 0.0f || x > 1.0f) ? 0.0f : sinf(x * PI); }

float heartLevel(uint32_t t) {                 // "lub-dub" at 50 bpm, wide and bright beats
  float p = (float)(t % 1200);
  float l = max(bump(p / 260.0f), 0.75f * bump((p - 340.0f) / 260.0f));
  // the first light is the strong beat; the soft light between beats only comes after the first one
  float base = 0.35f * constrain((t - 150.0f) / 250.0f, 0.0f, 1.0f);
  return max(base, l);
}

float breathe(uint32_t t) {                    // slow breathing, 6 s period
  return 0.6f + 0.4f * sinf(2.0f * PI * (float)(t % 6000) / 6000.0f);
}

float breatheSoft(uint32_t t) {                // "thinking of you" breathing: low and shallow
  return GLOW_LEVEL * (1.0f + 0.18f * sinf(2.0f * PI * (float)(t % 6000) / 6000.0f));
}

float envelope(uint32_t t, uint32_t dur, uint32_t in, uint32_t out) {
  if (t >= dur) return 0.0f;
  return min(1.0f, t / (float)in) * min(1.0f, (dur - t) / (float)out);
}

RGB mix(RGB a, RGB b, float k) {
  k = constrain(k, 0.0f, 1.0f);
  return { (uint8_t)(a.r + (b.r - a.r) * k),
           (uint8_t)(a.g + (b.g - a.g) * k),
           (uint8_t)(a.b + (b.b - a.b) * k) };
}

RGB hue(uint16_t h) {
  uint32_t c = Adafruit_NeoPixel::ColorHSV(h);
  return { (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c };
}

// --- sparks (fireworks) ---
void sparkReset() { memset(spLvl, 0, sizeof(spLvl)); }

// A new spark on top of one still lit blends with it instead of erasing it
void sparkSet(int i, RGB c, float l) {
  i = wrap(i);
  if (spLvl[i] > 0.15f) {
    spCol[i] = mix(spCol[i], c, l / (l + spLvl[i]));
    spLvl[i] = max(spLvl[i], l);
  } else if (l >= spLvl[i]) {
    spLvl[i] = l; spCol[i] = c;
  }
}

// pal = possible colors (repeat a color to make it more frequent)
// every = average interval between two bursts (ms)
void sparkStep(const RGB* pal, int n, uint32_t every, float level) {
  static uint32_t nextGap = 0;
  uint32_t now = millis();
  while (now - lastDecay >= 20) {               // 50 Hz decay, independent of the loop
    lastDecay += 20;
    if (now - lastDecay > 1000) lastDecay = now;
    for (int i = 0; i < NUM_LEDS; i++) spLvl[i] *= 0.95f;
  }
  if (now - lastSpark >= nextGap) {
    lastSpark = now;
    nextGap = every / 2 + random(every);          // irregular rhythm, like real fireworks
    int c = random(NUM_LEDS);
    RGB a = pal[random(n)], b = pal[random(n)];
    switch (random(100) / 25) {
      case 0:                                     // small spark
        sparkSet(c, a, 1.0f); sparkSet(c - 1, a, 0.4f); sparkSet(c + 1, a, 0.4f);
        break;
      case 1:                                     // wide burst
        sparkSet(c, a, 1.0f);
        sparkSet(c - 1, a, 0.7f); sparkSet(c + 1, a, 0.7f);
        sparkSet(c - 2, a, 0.35f); sparkSet(c + 2, a, 0.35f);
        break;
      case 2:                                     // two colors: center of one, edges of the other
        sparkSet(c, a, 1.0f);
        sparkSet(c - 1, mix(a, b, 0.6f), 0.7f); sparkSet(c + 1, mix(a, b, 0.6f), 0.7f);
        sparkSet(c - 2, b, 0.4f); sparkSet(c + 2, b, 0.4f);
        break;
      default:                                    // two bursts at once in different places
        sparkSet(c, a, 1.0f); sparkSet(c + 1, a, 0.4f);
        sparkSet(c + NUM_LEDS / 2, b, 1.0f); sparkSet(c + NUM_LEDS / 2 - 1, b, 0.4f);
        break;
    }
  }
  // the spark covers the background instead of adding to it: green on a red background stays green
  for (int i = 0; i < NUM_LEDS; i++) {
    float l = constrain(spLvl[i] * level, 0.0f, 1.0f);
    if (l < 0.01f) continue;
    float keep = 1.0f - l;
    acc[i][0] = acc[i][0] * keep + spCol[i].r * l * l;
    acc[i][1] = acc[i][1] * keep + spCol[i].g * l * l;
    acc[i][2] = acc[i][2] * keep + spCol[i].b * l * l;
  }
}

// Slow wave between a color and its "companion": the light is never flat
void livelyFill(RGB c, RGB c2, float level, uint32_t t) {
  for (int i = 0; i < NUM_LEDS; i++) {
    float ph = i / (float)NUM_LEDS + t / 9000.0f;
    float k = 0.22f + 0.22f * sinf(2.0f * PI * ph);
    putPx(i, mix(c, c2, k), level);
  }
}

// ======================= SPECIAL DATES =======================
bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool isMonthDay(const struct tm& t) {          // the 30th, in February the last day
  if (t.tm_mon == 1) return t.tm_mday == (isLeap(t.tm_year + 1900) ? 29 : 28);
  return t.tm_mday == 30;
}

bool isAnniversaryDay() { return timeOk && nowTm.tm_mon == ANNIV_MONTH - 1 && nowTm.tm_mday == ANNIV_DAY; }

bool redMode() {                               // the 30th of the month: red all day long
  if (redPreview) return true;
  return timeOk && isMonthDay(nowTm);
}

void startEvent(Anim a) {
  if (glowOn) {                                   // a special animation ends "thinking of you"
    if (!glowSeen) pendingOn = true;              // ...but if it was not seen, the LED stays on
    glowOn = false; glowStopping = false;
  }
  evAnim = a; evStart = millis();
  sparkReset();
  Serial.printf("Special animation %d\n", a);
}

void updateClock() {
  static uint32_t last = 0;
  if (last != 0 && millis() - last < 1000) return;
  last = millis();
  bool wasOk = timeOk;
  timeOk = getLocalTime(&nowTm, 0);
  if (timeOk && !wasOk)
    Serial.printf("Time synced: %02d/%02d/%04d %02d:%02d\n", nowTm.tm_mday,
                  nowTm.tm_mon + 1, nowTm.tm_year + 1900, nowTm.tm_hour, nowTm.tm_min);
}

void checkDates() {
  static int lastKey = -1;
  if (!timeOk) return;
  int key = nowTm.tm_yday * 1440 + nowTm.tm_hour * 60 + nowTm.tm_min;
  if (key == lastKey) return;                    // check once a minute
  lastKey = key;
  for (const SpecialDate& d : DATES) {
    if (d.day == nowTm.tm_mday && d.month == nowTm.tm_mon + 1 &&
        d.hour == nowTm.tm_hour && d.minute == nowTm.tm_min) { startEvent(d.anim); return; }
  }
  if (isMonthDay(nowTm) && nowTm.tm_hour == MONTH_HOUR && nowTm.tm_min == MONTH_MIN) startEvent(AN_MONTH);
}

void renderEvent(uint32_t t) {
  uint32_t dur = ANIM_MS[evAnim];
  float env = envelope(t, dur, 1500, 4000);
  switch (evAnim) {
    case AN_ANNIV: {                             // breathing burgundy base + yellow, gold, green and red fireworks
      for (int i = 0; i < NUM_LEDS; i++) {
        float w = 0.85f + 0.15f * sinf(2.0f * PI * (i / (float)NUM_LEDS + t / 7000.0f));
        addPx(i, RED_LOVE, 0.48f * env * w * breathe(t));
      }
      const RGB pal[] = {COL(C_YELLOW), COL(C_YELLOW), COL(C_GOLD), COL(C_GOLD), COL(C_YELLOW2),
                         COL(C_GREEN), COL(C_GREEN2), COL(C_CHAMPAGNE), RED_LOVE};
      sparkStep(pal, 9, 160, env);
      break;
    }
    case AN_MONTH: {                             // the red makes room for two comets (green and yellow), then comes back
      float u = t / (float)dur;
      float in = min(1.0f, t / 1500.0f);
      float tail = 1.0f - constrain((u - 0.55f) / 0.25f, 0.0f, 1.0f);   // the comets fade away
      float red  = max(1.0f - in, constrain((u - 0.6f) / 0.4f, 0.0f, 1.0f));  // the red goes out and comes back
      float pa = fmodf(t / 2400.0f, 1.0f) * NUM_LEDS;
      float pb = NUM_LEDS - pa;
      for (int k = 0; k < 6; k++) {
        float l = (1.0f - k / 6.0f) * tail * in;
        addPx((int)pa - k, COLOR_A, 0.85f * l);  // green is stronger: keep it a bit lower
        addPx((int)pb + k, k == 0 ? COL(C_GOLD) : COLOR_B, l);   // brighter head on the yellow comet
      }
      for (int i = 0; i < NUM_LEDS; i++) addPx(i, RED_LOVE, RED_LEVEL * red);
      break;
    }
    case AN_BDAY_B: {                            // Cate's party: yellow, orange and gold
      livelyFill(COL(C_YELLOW), COL(C_YELLOW2), 0.6f * env * breathe(t), t);
      const RGB pal[] = {COL(C_YELLOW), COL(C_YELLOW), COL(C_YELLOW2), COL(C_GOLD), COL(C_GOLD),
                         COL(C_ORANGE), COL(C_CHAMPAGNE)};
      sparkStep(pal, 7, 140, env);
      break;
    }
    case AN_BDAY_A: {                            // Andre's party: green, lime and a little gold
      livelyFill(COL(C_GREEN), COL(C_GREEN2), 0.55f * env * breathe(t), t);
      const RGB pal[] = {COL(C_GREEN), COL(C_GREEN), COL(C_GREEN2), COL(C_GREEN2), COL(C_GOLD),
                         COL(C_CHAMPAGNE)};
      sparkStep(pal, 6, 140, env);
      break;
    }
    case AN_VALENTINE: {                         // burgundy heartbeat with pink sparks
      for (int i = 0; i < NUM_LEDS; i++) addPx(i, RED_LOVE, 0.9f * env * heartLevel(t));
      const RGB pal[] = {COL(C_PINK), COL(C_PINK), COL(C_CHAMPAGNE)};
      sparkStep(pal, 3, 260, 0.85f * env);
      break;
    }
    case AN_XMAS: {                              // mostly red, fir green and gold
      for (int i = 0; i < NUM_LEDS; i++) addPx(i, RED_LOVE, 0.55f * env * breathe(t));
      const RGB pal[] = {RED_LOVE, COL(C_FIR), COL(C_FIR), COL(C_FIR), COL(C_GOLD), COL(C_GOLD),
                         COL(C_WHITE)};
      sparkStep(pal, 7, 150, env);
      break;
    }
    case AN_NEWYEAR: {                           // fireworks: mostly gold, red and champagne
      const RGB pal[] = {COL(C_GOLD), COL(C_GOLD), COL(C_GOLD), COL(C_CHAMPAGNE), COL(C_WHITE),
                         RED_LOVE, RED_LOVE, COL(C_ORANGE), COL(C_YELLOW), COL(C_GREEN),
                         COL(C_PINK), {40, 70, 255}};
      sparkStep(pal, 12, 90, env);
      break;
    }
    default: break;
  }
}

// ======================= RENDER =======================
void startGlow() {
  glowOn = true; glowStart = millis(); glowSeen = false; glowStopping = false; gnOn = false;
}
// "Together": you both touched, so you have both seen it: it turns off when the animation ends
void startTogether() {
  togetherOn = true; togetherStart = millis();
  glowOn = false; glowStopping = false;
}

void render() {
  uint32_t now = millis();
  if (togetherOn && now - togetherStart > TOGETHER_MS)   togetherOn = false;
  if (feedbackOn && now - feedbackStart > 1200)          feedbackOn = false;
  if (gnFbOn     && now - gnFbStart     > 3000)          gnFbOn = false;
  if (heartRxOn  && now - heartRxStart  > HEART_TIMEOUT) heartRxOn = false;
  if (gnOn       && now - gnStart       > GOODNIGHT_MS)  gnOn = false;
  if (redPreview && now - redPreviewStart > 60000)       redPreview = false;
  if (showColor >= 0 && now - showStart > 60000)         showColor = -1;
  if (evAnim != AN_NONE && now - evStart > ANIM_MS[evAnim]) evAnim = AN_NONE;
  if (glowOn && glowStopping && now - glowStopStart > GLOW_STOP_MS) { glowOn = false; glowStopping = false; }
  if (glowOn && now - glowStart > GLOW_MS) {
    glowOn = false;
    if (!glowSeen) pendingOn = true;             // nobody saw it: leave the LED on
  }

  clr();
  bool active = true;
  if (togetherOn) {
    uint32_t t = now - togetherStart;
    float lvl = min(1.0f, t / 800.0f) * breathe(now);
    for (int i = 0; i < NUM_LEDS; i++) {
      float ph = fmodf(i / (float)NUM_LEDS + t / 4000.0f, 1.0f);
      float k = 0.5f + 0.5f * sinf(2.0f * PI * ph);
      addPx(i, mix(MY_COLOR, OTHER_COLOR, k), lvl);
    }
  } else if (heartTx) {
    fillPx(MY_COLOR, 0.85f * heartLevel(now - heartTxStart));      // you are sending the heartbeat
  } else if (feedbackOn) {
    fillPx(MY_COLOR, bump((now - feedbackStart) / 1200.0f));       // tap sent
  } else if (gnFbOn) {
    float u = (now - gnFbStart) / 3000.0f;                          // goodnight sent
    fillPx(mix(COL(C_ORANGE), COL(C_EMBER), u), 0.7f * bump(u * 0.5f + 0.5f) * min(1.0f, u * 8.0f));
  } else if (heartRxOn) {
    fillPx(OTHER_COLOR, heartLevel(now - heartRxStart));
  } else if (evAnim != AN_NONE) {
    renderEvent(now - evStart);
  } else if (gnOn) {                                                // sunset: from warm amber to ember
    uint32_t t = now - gnStart;
    float u = t / (float)GOODNIGHT_MS;
    const RGB AMBER = {255, 105, 10};
    RGB c = (u < 0.45f) ? mix(AMBER, COL(C_ORANGE), u / 0.45f)
                        : mix(COL(C_ORANGE), COL(C_EMBER), (u - 0.45f) / 0.55f);
    livelyFill(c, COL(C_EMBER), min(1.0f, t / 2000.0f) * 0.9f * powf(1.0f - u, 1.2f), t);
  } else if (glowOn) {
    uint32_t t = now - glowStart;
    float fadeIn  = min(1.0f, t / 1500.0f);
    float fadeOut = min(1.0f, (float)(GLOW_MS - t) / 4000.0f);
    float stop = glowStopping ? max(0.0f, 1.0f - (now - glowStopStart) / (float)GLOW_STOP_MS) : 1.0f;
    float lvl = fadeIn * fadeOut * stop * breatheSoft(now);
    livelyFill(OTHER_COLOR, OTHER_COLOR2, lvl, t);
    if (isAnniversaryDay()) {                                       // on the anniversary "thinking of you" is more festive
      const RGB pal[] = {COLOR_A, COLOR_B, RED_LOVE, COL(C_GOLD)};
      sparkStep(pal, 4, 350, fadeIn * fadeOut);
    }
  } else {
    active = false;
    if (showColor >= 0)  fillPx(COL(showColor), showColor == C_RED ? RED_LEVEL : 0.8f);
    else if (redMode())  fillPx(RED_LOVE, RED_LEVEL);
    if (pendingOn) putPx(PENDING_LED, OTHER_COLOR,
                         0.58f + 0.14f * sinf(2.0f * PI * (float)(now % 5000) / 5000.0f));
  }

  if (active) lastLit = now;
  else updateAmbient();
  flush(!active && !redMode() && showColor < 0 && !mqtt.connected());
}

// Reads the photoresistor only when no animation is running, so it does not measure its own light
void updateAmbient() {
#if USE_LDR
  static uint32_t lastRead = 0;
  uint32_t now = millis();
  if (now - lastLit < 1500 || now - lastRead < 100) return;
  lastRead = now;
  int raw = analogRead(LDR_PIN);
  ldrSmooth = (ldrSmooth < 0) ? raw : ldrSmooth * 0.95f + raw * 0.05f;
  float k = constrain((ldrSmooth - LDR_DARK) / (float)(LDR_BRIGHT - LDR_DARK), 0.0f, 1.0f);
  ambient = NIGHT_LEVEL + (1.0f - NIGHT_LEVEL) * k;
#endif
}

// ======================= MQTT =======================
// Commands, status strings and the replies on /log are in Italian, as on the deployed lamps
// (translated in docs/MQTT_COMMANDS.md).
bool recent(bool ever, uint32_t t, uint32_t win) { return ever && millis() - t < win; }

void send(const char* msg) {
  if (mqtt.connected()) mqtt.publish(pubTopic.c_str(), msg);
}

void publishStatus() {
  if (!mqtt.connected()) return;
  String s = String("online v") + FW_VERSION + " rssi=" + WiFi.RSSI();
  mqtt.publish(statTopic.c_str(), s.c_str(), true);
}

int colorIndex(const String& name) {
  for (int i = 0; i < C_N; i++) if (name == PAL[i].name) return i;
  return -1;
}

void logMsg(const String& m) {
  Serial.println(m);
  if (mqtt.connected()) mqtt.publish(logTopic.c_str(), m.c_str());
}

void handleCommand(const String& msg) {
  if (msg == "update") {
    otaRequested = true;                         // run it in the loop, not inside the callback
  } else if (msg == "ping") {
    publishStatus();
  } else if (msg == "riavvia") {                  // restart
    ESP.restart();
  } else if (msg.startsWith("colore ")) {         // colore <name> R G B  (lasts until restart)
    char name[16]; int r, g, b;
    if (sscanf(msg.c_str() + 7, "%15s %d %d %d", name, &r, &g, &b) == 4 && colorIndex(name) >= 0) {
      int i = colorIndex(name);
      PAL[i].c = { (uint8_t)constrain(r, 0, 255), (uint8_t)constrain(g, 0, 255), (uint8_t)constrain(b, 0, 255) };
      showColor = i; showStart = millis();
      logMsg(String("{\"") + name + "\", {" + r + ", " + g + ", " + b + "}},");
    } else {
      logMsg("uso: colore <nome> R G B");
    }
  } else if (msg.startsWith("mostra ")) {         // mostra <name>: show a solid color for 60 s
    int i = colorIndex(msg.substring(7));
    if (i >= 0) { showColor = i; showStart = millis(); }
  } else if (msg.startsWith("livello_rosso ")) {  // red level
    RED_LEVEL = constrain(msg.substring(14).toFloat(), 0.05f, 1.0f);
    showColor = C_RED; showStart = millis();
    logMsg(String("RED_LEVEL = ") + RED_LEVEL);
  } else if (msg.startsWith("livello_respiro ")) { // brightness of "thinking of you", lasts until restart
    GLOW_LEVEL = constrain(msg.substring(16).toFloat(), 0.1f, 0.85f);
    logMsg(String("GLOW_LEVEL = ") + GLOW_LEVEL);
  } else if (msg == "luce") {                     // photoresistor reading
    logMsg(String("luce=") + (int)ldrSmooth + " ambient=" + ambient +
           " (buio sotto " + LDR_DARK + ", piena luce sopra " + LDR_BRIGHT + ")");
  } else if (msg.startsWith("ldr_buio ")) {       // dark threshold
    LDR_DARK = msg.substring(9).toInt();
    logMsg(String("LDR_DARK = ") + LDR_DARK);
  } else if (msg.startsWith("ldr_luce ")) {       // bright threshold
    LDR_BRIGHT = max(LDR_DARK + 1, (int)msg.substring(9).toInt());
    logMsg(String("LDR_BRIGHT = ") + LDR_BRIGHT);
  } else if (msg == "touch") {                    // sensor readings, to tune it remotely
    logMsg(String("touch: riposo ") + touchBaseline + ", minimo da ultimo controllo " + touchMin +
           ", soglia " + touchBaseline * touchRatio + " (touch_ratio " + touchRatio + ")");
    touchMin = 1e9;
  } else if (msg.startsWith("touch_ratio ")) {
    touchRatio = constrain(msg.substring(12).toFloat(), 0.3f, 0.95f);
    logMsg(String("touch_ratio = ") + touchRatio);
  } else if (msg == "colori") {                   // color list, ready to paste into the code
    String out;
    for (int i = 0; i < C_N; i++)
      out += String("{\"") + PAL[i].name + "\", {" + PAL[i].c.r + ", " + PAL[i].c.g + ", " + PAL[i].c.b + "}},\n";
    logMsg(out);
  } else if (msg.startsWith("prova ")) {          // prova <animation>: test an animation now
    String a = msg.substring(6);
    if      (a == "anniversario")     startEvent(AN_ANNIV);
    else if (a == "mese")             startEvent(AN_MONTH);
    else if (a == "compleanno_cate")  startEvent(AN_BDAY_B);
    else if (a == "compleanno_andre") startEvent(AN_BDAY_A);
    else if (a == "san_valentino")    startEvent(AN_VALENTINE);
    else if (a == "natale")           startEvent(AN_XMAS);
    else if (a == "capodanno")        startEvent(AN_NEWYEAR);
    else if (a == "rosso")            { redPreview = true; redPreviewStart = millis(); }
    else if (a == "buonanotte")       { gnOn = true; gnStart = millis(); glowOn = false; }
    else if (a == "penso")            pendingOn = true;
  }
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  String msg;
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  Serial.printf("Received on %s: %s\n", topic, msg.c_str());

  if (cmdTopic == topic) { handleCommand(msg); return; }

  if (msg == "tap") {
    tapRxEver = true; lastTapRx = millis();
    if (recent(tapSentEver, lastTapSent, TOGETHER_WIN)) startTogether();
    else startGlow();
  } else if (msg == "heart_on") {
    heartRxOn = true; heartRxStart = millis();
  } else if (msg == "heart_off") {
    heartRxOn = false; startGlow();
  } else if (msg == "goodnight") {
    glowOn = false; heartRxOn = false; togetherOn = false;
    gnOn = true; gnStart = millis();
  }
}

void mqttConnect() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (mqttAttempted && millis() - lastMqttAttempt < 5000) return;
  mqttAttempted = true;
  lastMqttAttempt = millis();

  String id = String("lampada-") + LAMP_ID + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqtt.connect(id.c_str(), MQTT_USER, MQTT_PASS, statTopic.c_str(), 1, true, "offline")) {
    mqtt.subscribe(subTopic.c_str());
    mqtt.subscribe(cmdTopic.c_str());
    if (otaError.length()) {                     // first the update error, if any, then the status
      mqtt.publish(logTopic.c_str(), ("aggiornamento fallito: " + otaError).c_str());
      otaError = "";
    }
    publishStatus();
    Serial.println("MQTT connected");
    if (!appMarkedValid) {                       // the firmware works: no rollback
      esp_ota_mark_app_valid_cancel_rollback();
      appMarkedValid = true;
    }
  } else {
    Serial.printf("MQTT error %d\n", mqtt.state());
  }
}

// ======================= REMOTE UPDATE (OTA) =======================
uint32_t otaCur = 0, otaTotal = 0;

// Error names for the /log topic, in Italian like on the deployed lamps (see docs/OTA_UPDATES.md)
String updateErrorName(int code) {
  static const char* names[] = {"ok", "scrittura flash", "cancellazione flash", "lettura flash",
    "spazio insufficiente", "dimensione non valida", "download interrotto o troppo lento",
    "MD5 errato", "file non valido (non è un firmware)", "attivazione", "nessuna partizione OTA",
    "argomento non valido", "annullato", "decifratura", "firma", "SHA256", "SHA512"};
  if (code >= 0 && code <= 16) return names[code];
  return String("codice ") + code;
}

bool otaAttempt(const String& url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPUpdate upd(30000);                          // 30 s HTTP timeout (slow connections)
  upd.rebootOnUpdate(true);
  upd.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  otaCur = otaTotal = 0;
  upd.onProgress([](int cur, int total) {         // light blue progress bar
    otaCur = cur; otaTotal = total;
    int n = total > 0 ? (int)((long long)cur * NUM_LEDS / total) : 0;
    for (int i = 0; i < NUM_LEDS; i++)
      strip.setPixelColor(i, i < n ? strip.Color(0, 30, 60) : 0);
    strip.show();
  });

  HTTPUpdateResult r = upd.update(client, url);   // if it works, the board restarts by itself
  int code = upd.getLastError();
  if (r == HTTP_UPDATE_NO_UPDATES)  otaError = "nessun aggiornamento";
  else if (code > 0)                otaError = updateErrorName(code);
  else                              otaError = upd.getLastErrorString();
  otaError += String(" (scaricati ") + otaCur / 1024 + "/" + otaTotal / 1024 + " KB, rssi=" + WiFi.RSSI() + ")";
  Serial.printf("Update failed: %s\n", otaError.c_str());
  return false;
}

void doOta() {
  otaRequested = false;
  String url = String(FW_BASE) + "lampada_" + LAMP_ID + ".bin";
  Serial.printf("Updating from %s (rssi=%d)\n", url.c_str(), WiFi.RSSI());
  if (mqtt.connected()) {
    mqtt.publish(statTopic.c_str(), "aggiornamento in corso", true);
    mqtt.disconnect();                           // frees memory for the second TLS connection
  }
  net.stop();

  String errors;
  for (int attempt = 1; attempt <= 3; attempt++) {  // up to 3 attempts
    otaAttempt(url);
    errors += String("tentativo ") + attempt + ": " + otaError + "; ";
    delay(2000);
  }
  otaError = errors;
  for (int i = 0; i < 3; i++) {                        // three red flashes
    for (int j = 0; j < NUM_LEDS; j++) strip.setPixelColor(j, 60, 0, 0);
    strip.show(); delay(200);
    strip.clear(); strip.show(); delay(200);
  }
  mqttAttempted = false;                               // reconnect right away
}

// ======================= TOUCH =======================
float readTouch() {
  uint32_t s = 0;
  for (int i = 0; i < 4; i++) s += touchRead(TOUCH_PIN);
  return s / 4.0f;
}

void calibrateTouch() {
  float s = 0;
  for (int i = 0; i < 40; i++) { s += touchRead(TOUCH_PIN); delay(10); }
  touchBaseline = s / 40.0f;
}

void userTouched() {                             // every touch counts as "seen"
  pendingOn = false;
  if (glowOn && !glowStopping) {                 // touched while "breathing": slow fade-out
    glowSeen = true;
    glowStopping = true; glowStopStart = millis();
  }
}

void sendTap() {
  uint32_t now = millis();
  if (tapSentEver && now - lastTapSent < TAP_COOLDOWN) return;
  tapSentEver = true; lastTapSent = now;
  send("tap");
  feedbackOn = true; feedbackStart = now;
  if (recent(tapRxEver, lastTapRx, TOGETHER_WIN)) startTogether();
}

void sendGoodnight() {
  send("goodnight");
  feedbackOn = false; togetherOn = false;
  gnFbOn = true; gnFbStart = millis();
}

void handleTouch() {
  static uint32_t lastPrint = 0;
  uint32_t now = millis();
  float v = readTouch();
  bool below = v < touchBaseline * touchRatio;                  // finger on
  bool above = v > touchBaseline * min(0.97f, touchRatio + 0.08f);  // finger off (with hysteresis)
  touchMin = min(touchMin, v);

  if (now - lastPrint > 1000) {                 // handy for tuning the touch
    Serial.printf("touch=%.1f baseline=%.1f  light=%.0f ambient=%.2f\n",
                  v, touchBaseline, ldrSmooth, ambient);
    lastPrint = now;
  }

  if (!touching) {
    if (below) {
      if (!candidate) { candidate = true; candStart = now; }
      else if (now - candStart >= PRESS_MS) {   // touch confirmed
        candidate = false;
        touching = true; pressStart = candStart; lastTouchSeen = now;
        userTouched();
        secondPress = tapWaiting && candStart - tapReleaseAt < DOUBLE_WIN;
        tapWaiting = false;
      }
    } else {
      candidate = false;
      touchBaseline = touchBaseline * 0.999f + v * 0.001f;     // slow recalibration at rest
    }
  } else {
    if (!above) lastTouchSeen = now;
    if (!heartTx && now - pressStart > HOLD_MS) {
      heartTx = true; heartTxStart = now; secondPress = false;
      send("heart_on");
    }
    if (now - lastTouchSeen > RELEASE_MS) {
      touching = false;                         // released
      if (heartTx) {
        heartTx = false;
        send("heart_off");
      } else if (secondPress) {
        secondPress = false;
        sendGoodnight();
      } else {
        sendTap();                              // the tap is sent right away
        tapWaiting = true; tapReleaseAt = now;  // if a second tap comes, it becomes "goodnight"
      }
    }
  }

  if (tapWaiting && now - tapReleaseAt >= DOUBLE_WIN) tapWaiting = false;
}

// ======================= SETUP / LOOP =======================
void setup() {
  Serial.begin(115200);
  Serial.printf("\nLamp %c, firmware v%s (%s)\n", LAMP_ID, FW_VERSION, LAMP_TAG);
  strip.begin();
  strip.setBrightness(MAX_BRIGHT);
  strip.clear(); strip.show();
  randomSeed(esp_random());

  pubTopic  = String(TOPIC_BASE) + LAMP_ID;
  subTopic  = String(TOPIC_BASE) + OTHER_ID;
  cmdTopic  = pubTopic + "/cmd";
  statTopic = pubTopic + "/stato";
  logTopic  = pubTopic + "/log";

  // If there is no saved network it creates the "Lampada di Andre" / "❤Lampada di Cate❤" network:
  // connect to it with a phone and pick the home Wi-Fi. Light blue ring = waiting.
  WiFiManager wm;
  wm.setAPCallback([](WiFiManager*) {
    for (int i = 0; i < NUM_LEDS; i++) strip.setPixelColor(i, 0, 0, 40);
    strip.show();
  });
  wm.setConfigPortalTimeout(300);
  const char* ap = (LAMP_ID == 'A') ? AP_NAME_A : AP_NAME_B;
  wm.setTitle(ap);                               // title of the setup page
  if (!wm.autoConnect(ap, AP_PASS)) ESP.restart();
  WiFi.setAutoReconnect(true);

  configTzTime(TZ_ITALY, "pool.ntp.org", "time.google.com");

  net.setInsecure();                 // TLS without certificate check: fine for this use
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  mqtt.setKeepAlive(30);

  // short startup animation in the lamp's own color
  clr();
  for (int i = 0; i < NUM_LEDS; i++) { putPx(i, MY_COLOR, 0.8f); flush(false); delay(40); }
  delay(300);
  clr(); flush(false);

  calibrateTouch();                  // do not touch the lamp while it starts up
}

void loop() {
  mqttConnect();
  mqtt.loop();
  if (otaRequested) doOta();
  updateClock();
  checkDates();
  handleTouch();
  render();
  delay(10);
}
