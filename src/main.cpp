#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

// Режимы: что крутит энкодер
enum Mode : uint8_t {
  MODE_BRIGHT,     // яркость основного света
  MODE_CT,         // цветовая температура основного света
  MODE_BG_BRIGHT,  // яркость задней подсветки
  MODE_BG_HUE,     // цвет задней подсветки
  MODE_COUNT
};

struct LampState {
  int bright   = 50;
  int ct       = 4500;
  int bgBright = 50;
  int bgHue    = 0;
} lamp;

Mode mode = MODE_BRIGHT;
int  pending = -1;           // какое значение ждёт отправки (-1 = ничего)
uint32_t lastSend = 0;
uint32_t lastWifiTry = 0;

// ---------- Светодиод-индикатор режима ----------

void showMode() {
  uint8_t r = 0, g = 0, b = 0;
  switch (mode) {
    case MODE_BRIGHT:    r = g = b = LED_LEVEL; break;       // белый
    case MODE_CT:        r = LED_LEVEL; g = LED_LEVEL / 3; break; // тёплый
    case MODE_BG_BRIGHT: r = b = LED_LEVEL; break;           // фиолетовый
    case MODE_BG_HUE:    g = b = LED_LEVEL; break;           // бирюзовый
    default: break;
  }
  neopixelWrite(LED_PIN, r, g, b);
}

void flashError() {
  neopixelWrite(LED_PIN, LED_LEVEL * 2, 0, 0);
  delay(150);
  showMode();
}

// ---------- Связь с лампой ----------

// Отправляет одну команду, возвращает первую строку ответа
String sendCmd(const String &json) {
  WiFiClient c;
  c.setTimeout(1);  // секунды
  if (!c.connect(LAMP_IP, LAMP_PORT, 1000)) {
    Serial.println("lamp: нет соединения");
    flashError();
    return "";
  }
  c.print(json);
  c.print("\r\n");
  String reply = c.readStringUntil('\n');
  c.stop();
  Serial.printf("-> %s\n<- %s\n", json.c_str(), reply.c_str());
  if (reply.indexOf("error") >= 0) flashError();
  return reply;
}

void sendToggle(bool backlight) {
  sendCmd(String("{\"id\":1,\"method\":\"") + (backlight ? "bg_toggle" : "toggle") + "\",\"params\":[]}");
}

void sendValue(Mode m) {
  String cmd;
  switch (m) {
    case MODE_BRIGHT:
      cmd = String("{\"id\":1,\"method\":\"set_bright\",\"params\":[") + lamp.bright + ",\"smooth\",150]}";
      break;
    case MODE_CT:
      cmd = String("{\"id\":1,\"method\":\"set_ct_abx\",\"params\":[") + lamp.ct + ",\"smooth\",150]}";
      break;
    case MODE_BG_BRIGHT:
      cmd = String("{\"id\":1,\"method\":\"bg_set_bright\",\"params\":[") + lamp.bgBright + ",\"smooth\",150]}";
      break;
    case MODE_BG_HUE:
      cmd = String("{\"id\":1,\"method\":\"bg_set_hsv\",\"params\":[") + lamp.bgHue + ",100,\"smooth\",150]}";
      break;
    default:
      return;
  }
  sendCmd(cmd);
}

// Читаем текущие значения лампы при старте, чтобы пульт начинал с них
void syncFromLamp() {
  String r = sendCmd("{\"id\":1,\"method\":\"get_prop\",\"params\":[\"bright\",\"ct\",\"bg_bright\",\"bg_hue\"]}");
  int start = r.indexOf("\"result\":[");
  if (start < 0) return;
  start += 10;
  int end = r.indexOf(']', start);
  if (end < 0) return;
  String list = r.substring(start, end);
  list.replace("\"", "");

  int vals[4] = {0, 0, 0, 0};
  int idx = 0, from = 0;
  while (idx < 4) {
    int comma = list.indexOf(',', from);
    String part = (comma < 0) ? list.substring(from) : list.substring(from, comma);
    vals[idx++] = part.toInt();
    if (comma < 0) break;
    from = comma + 1;
  }
  if (vals[0] > 0) lamp.bright   = vals[0];
  if (vals[1] > 0) lamp.ct       = vals[1];
  if (vals[2] > 0) lamp.bgBright = vals[2];
  lamp.bgHue = constrain(vals[3], 0, 359);
}

// ---------- Энкодер ----------

int8_t readEncoder() {
  static const int8_t table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  static uint8_t state = 0;
  static int8_t acc = 0;

  state = ((state << 2) | (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B)) & 0x0F;
  acc += table[state];

  int8_t dir = 0;
  if (acc >= ENC_STEPS_PER_DETENT)  { acc = 0; dir = 1; }
  if (acc <= -ENC_STEPS_PER_DETENT) { acc = 0; dir = -1; }
#if ENC_REVERSE
  dir = -dir;
#endif
  return dir;
}

void applyRotation(int8_t dir) {
  switch (mode) {
    case MODE_BRIGHT:    lamp.bright   = constrain(lamp.bright   + dir * STEP_BRIGHT, 1, 100); break;
    case MODE_CT:        lamp.ct       = constrain(lamp.ct       + dir * STEP_CT, 2700, 6500); break;
    case MODE_BG_BRIGHT: lamp.bgBright = constrain(lamp.bgBright + dir * STEP_BRIGHT, 1, 100); break;
    case MODE_BG_HUE:    lamp.bgHue    = (lamp.bgHue + dir * STEP_HUE + 360) % 360; break;
    default: break;
  }
  pending = mode;
}

// ---------- Кнопка ----------

void handleButton() {
  static bool lastPressed = false;
  static uint32_t pressedAt = 0;
  static uint32_t changedAt = 0;

  bool pressed = (digitalRead(PIN_ENC_BTN) == LOW);
  uint32_t now = millis();
  if (pressed == lastPressed || now - changedAt < 30) return;  // антидребезг
  changedAt = now;
  lastPressed = pressed;

  if (pressed) {
    pressedAt = now;
    return;
  }

  // кнопку отпустили
  if (now - pressedAt >= LONG_PRESS_MS) {
    mode = (Mode)((mode + 1) % MODE_COUNT);
    showMode();
    Serial.printf("режим: %d\n", mode);
  } else {
    bool backlight = (mode == MODE_BG_BRIGHT || mode == MODE_BG_HUE);
    sendToggle(backlight);
  }
}

// ---------- Wi-Fi ----------

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Wi-Fi");
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  }
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  showMode();

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) syncFromLamp();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiTry > 5000) {
      lastWifiTry = millis();
      WiFi.reconnect();
    }
    return;
  }

  int8_t dir = readEncoder();
  if (dir != 0) applyRotation(dir);

  handleButton();

  // Не чаще раза в 100 мс, отправляем только последнее значение
  if (pending >= 0 && millis() - lastSend > 100) {
    sendValue((Mode)pending);
    pending = -1;
    lastSend = millis();
  }
}
