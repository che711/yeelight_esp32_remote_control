#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "index_html.h"

WebServer server(80);
uint32_t cmdId = 1;
uint32_t lastWifiTry = 0;

// ---------- Связь с лампой ----------

// Отправляет одну команду лампе, возвращает первую строку ответа
String sendCmd(const String &method, const String &params) {
  // Лампа принимает около 60 команд в минуту, иначе замолкает на минуту и дольше.
  // Держим паузу не меньше 1 секунды между командами.
  static uint32_t lastCmd = 0;
  while (lastCmd != 0 && millis() - lastCmd < 1000) delay(20);
  lastCmd = millis();

  WiFiClient c;
  if (!c.connect(LAMP_IP, LAMP_PORT, 2000)) {
    Serial.println("lamp: нет соединения (проверьте LAN Control и IP)");
    return "";
  }
  c.setNoDelay(true);
  uint32_t id = cmdId++;
  String idTag = String("\"id\":") + id + ",";
  String json = String("{\"id\":") + id + ",\"method\":\"" + method + "\",\"params\":" + params + "}";
  c.print(json + "\r\n");  // одним пакетом: лампа сбрасывает соединение, если «\r\n» приходит отдельно
  // Ждём ответ до 3 секунд, читаем до конца строки.
  // Таймаут задан явно: у setTimeout() в разных версиях ядра разные единицы (секунды или миллисекунды).
  // Лампа может прислать перед ответом уведомление {"method":"props",...} — такие строки пропускаем
  // и ждём строку с нашим id.
  String reply, line;
  uint32_t t0 = millis();
  bool got = false;
  while (!got && millis() - t0 < 3000 && (c.connected() || c.available())) {
    while (c.available()) {
      char ch = (char)c.read();
      if (ch == '\n') {
        if (line.indexOf(idTag) >= 0) { reply = line; got = true; break; }
        line = "";
      } else if (ch != '\r') {
        line += ch;
      }
    }
    if (!got) delay(5);
  }
  c.stop();
  Serial.printf("-> %s\n<- %s\n", json.c_str(), reply.c_str());
  if (reply.length() == 0) {
    Serial.println("lamp: соединение есть, но ответа нет (возможно, лимит команд, подождите минуту)");
  }
  return reply;
}

bool replyOk(const String &reply) {
  return reply.indexOf("\"ok\"") >= 0;
}

// Достаёт значения из {"id":1,"result":["60","4500",...]}
int parseResult(const String &reply, String out[], int maxCount) {
  int start = reply.indexOf("\"result\":[");
  if (start < 0) return 0;
  start += 10;
  int end = reply.indexOf(']', start);
  if (end < 0) return 0;
  String list = reply.substring(start, end);
  list.replace("\"", "");

  int count = 0, from = 0;
  while (count < maxCount) {
    int comma = list.indexOf(',', from);
    out[count++] = (comma < 0) ? list.substring(from) : list.substring(from, comma);
    if (comma < 0) break;
    from = comma + 1;
  }
  return count;
}

// ---------- Веб-сервер ----------

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleState() {
  String reply = sendCmd("get_prop",
      "[\"power\",\"bright\",\"ct\",\"bg_power\",\"bg_bright\",\"bg_hue\",\"bg_sat\",\"bg_flowing\"]");
  String v[8];
  int n = parseResult(reply, v, 8);
  if (n < 7) {
    server.send(200, "application/json", "{\"ok\":false}");
    return;
  }
  String j = "{\"ok\":true";
  j += ",\"power\":\"" + v[0] + "\"";
  j += ",\"bright\":" + String(v[1].toInt());
  j += ",\"ct\":" + String(v[2].toInt());
  j += ",\"bg_power\":\"" + v[3] + "\"";
  j += ",\"bg_bright\":" + String(v[4].toInt());
  j += ",\"hue\":" + String(v[5].toInt());
  j += ",\"sat\":" + String(v[6].toInt());
  j += ",\"bg_flow\":\"" + (n >= 8 ? v[7] : String("")) + "\"";  // "1" — переливается, "0" — нет
  j += "}";
  server.send(200, "application/json", j);
}

void handleSet() {
  bool allOk = true;
  bool lampSilent = false;  // лампа не ответила совсем: остальные команды пробовать нет смысла
  auto run = [&](const String &method, const String &params) {
    if (lampSilent) { allOk = false; return; }
    String reply = sendCmd(method, params);
    if (reply.length() == 0) lampSilent = true;
    if (!replyOk(reply)) allOk = false;
  };

  if (server.hasArg("power")) {
    run("set_power", String("[\"") + (server.arg("power") == "on" ? "on" : "off") + "\",\"smooth\",300]");
  }
  if (server.hasArg("bright")) {
    int v = constrain(server.arg("bright").toInt(), 1, 100);
    run("set_bright", "[" + String(v) + ",\"smooth\",150]");
  }
  if (server.hasArg("ct")) {
    int v = constrain(server.arg("ct").toInt(), 2700, 6500);
    run("set_ct_abx", "[" + String(v) + ",\"smooth\",150]");
  }
  bool bgDone = false;  // подсветка уже получила цвет и яркость вместе с включением
  if (server.hasArg("bg_power")) {
    if (server.arg("bg_power") == "on") {
      // Команда bg_set_power on у этой лампы отвечает «ok», но подсветку не включает.
      // Поэтому включаем сценой: она сама включает подсветку и задаёт цвет и яркость.
      int hue = server.hasArg("hue") ? constrain(server.arg("hue").toInt(), 0, 359) : 0;
      int sat = server.hasArg("sat") ? constrain(server.arg("sat").toInt(), 0, 100) : 100;
      int br  = server.hasArg("bg_bright") ? constrain(server.arg("bg_bright").toInt(), 1, 100) : 100;
      run("bg_set_scene", "[\"hsv\"," + String(hue) + "," + String(sat) + "," + String(br) + "]");
      bgDone = true;
      // Проверяем, что подсветка правда включилась; если нет, переключаем командой bg_toggle
      if (!lampSilent) {
        String v[1];
        String st = sendCmd("get_prop", "[\"bg_power\"]");
        if (st.length() == 0) lampSilent = true;
        else if (parseResult(st, v, 1) == 1 && v[0] != "on") run("bg_toggle", "[]");
      }
    } else {
      run("bg_set_power", "[\"off\",\"smooth\",300]");
    }
  }
  // Переливание: лампа сама плавно крутит цвета по кругу, плата после запуска ей не нужна.
  // Запускаем сценой «cf», как и обычное включение подсветки: она заодно включает подсветку.
  if (server.hasArg("bg_flow")) {
    if (server.arg("bg_flow") == "on") {
      int stepMs = constrain(server.arg("flow_ms").toInt(), 500, 30000);  // время смены одного цвета
      int br = server.hasArg("bg_bright") ? constrain(server.arg("bg_bright").toInt(), 1, 100) : 100;
      const unsigned long colors[] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF};
      String flow = "";
      for (int i = 0; i < 6; i++) {
        if (i) flow += ",";
        // длительность, режим 1 (RGB), цвет, яркость
        flow += String(stepMs) + ",1," + String(colors[i]) + "," + String(br);
      }
      // 0 — крутить бесконечно, 1 — после остановки остаться на текущем цвете
      run("bg_set_scene", "[\"cf\",0,1,\"" + flow + "\"]");
      bgDone = true;
    } else {
      run("bg_stop_cf", "[]");
    }
  }
  if (server.hasArg("bg_bright") && !bgDone) {
    int v = constrain(server.arg("bg_bright").toInt(), 1, 100);
    run("bg_set_bright", "[" + String(v) + ",\"smooth\",150]");
  }
  if (server.hasArg("hue") && !bgDone) {
    int hue = constrain(server.arg("hue").toInt(), 0, 359);
    int sat = server.hasArg("sat") ? constrain(server.arg("sat").toInt(), 0, 100) : 100;
    run("bg_set_hsv", "[" + String(hue) + "," + String(sat) + ",\"smooth\",150]");
  }

  server.send(200, "application/json", allOk ? "{\"ok\":true}" : "{\"ok\":false}");
}

// ---------- Wi-Fi ----------

// Причина отключения: 2/202 — ошибка входа, 15 — не подошёл пароль,
// 201 — сеть не найдена, 205 — сбой соединения
void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    Serial.printf("\nпричина отключения: %d\n", info.wifi_sta_disconnected.reason);
  }
}

void connectWifi() {
  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(WIFI_HOSTNAME);  // так плата будет называться в списке роутера
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // на этой плате с полной мощностью радио не работает
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Wi-Fi");
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 20000) {
    delay(300);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    // 1 = сеть не найдена, 4 = неверный пароль
    Serial.printf("Wi-Fi статус: %d\n", WiFi.status());
    Serial.println("Сети, которые видит плата:");
    WiFi.disconnect();  // сканирование не идёт, пока плата пытается подключиться
    delay(500);
    int n = WiFi.scanNetworks();
    if (n <= 0) Serial.println("  (ничего не найдено)");
    for (int i = 0; i < n; i++) {
      Serial.printf("  \"%s\"  канал %d  %d dBm\n", WiFi.SSID(i).c_str(), WiFi.channel(i), WiFi.RSSI(i));
    }
    WiFi.scanDelete();  // освобождаем память под результаты сканирования
  }
}

// ---------- setup / loop ----------

void setup() {
  Serial.begin(115200);
  // USB-порт платы появляется после перезапуска не сразу: ждём монитор до 8 секунд
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 8000) delay(10);
  delay(500);
  Serial.println("\nстарт");

  connectWifi();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Страница: http://");
    Serial.println(WiFi.localIP());
    Serial.print("MAC платы: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("Wi-Fi не подключился, проверьте config.h");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/set", HTTP_GET, handleSet);
  server.begin();
}

void loop() {
  server.handleClient();

  // Раз в 10 секунд сообщаем состояние, чтобы адрес был виден, даже если монитор открыли поздно
  static uint32_t lastInfo = 0;
  if (millis() - lastInfo > 10000) {
    lastInfo = millis();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Wi-Fi ок, страница: http://%s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.printf("Wi-Fi не подключён, статус %d\n", WiFi.status());
    }
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTry > 20000) {
    lastWifiTry = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
}
