#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"
#include "index_html.h"

WebServer server(80);
Preferences prefs;  // память платы, которая переживает перезагрузку: тут хранятся сцены
uint32_t cmdId = 1;
uint32_t lastWifiTry = 0;

// Какой эффект запускали на подсветке и с какими настройками. Лампа сообщает только «эффект идёт или нет»,
// а после перезагрузки страницы нужно показать, какой именно, поэтому помним это на плате.
String bgEffect = "off";  // off, on (переливание), wave, fire
int bgFlowMs = 3000;
int bgFlowHue = 0;
int bgFlowSat = 100;
int bgFlowSwing = 0;       // размах яркости у переливаний, в процентах
uint32_t sleepEndMs = 0;   // когда по времени платы подсветка погаснет по таймеру сна (нужно только чтобы показать остаток)

// Занятость сервера: какую долю времени за последние 10 секунд плата обрабатывала запросы
uint32_t busyUs = 0;
uint32_t busyWindowStart = 0;
uint8_t busyPct = 0;

// ---------- Связь с лампой ----------

// Отправляет одну команду лампе, возвращает первую строку ответа
String sendCmd(const String &method, const String &params, uint32_t gapMs = 1000) {
  // Лампа принимает около 60 команд в минуту, иначе замолкает на минуту и дольше.
  // Обычно держим паузу не меньше 1 секунды между командами; короткая пауза нужна только для пары
  // «выключить основной свет — вернуть подсветку», такая пара не сбивает минутный счёт.
  static uint32_t lastCmd = 0;
  while (lastCmd != 0 && millis() - lastCmd < gapMs) delay(10);
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
      "[\"power\",\"bright\",\"ct\",\"bg_power\",\"bg_bright\",\"bg_hue\",\"bg_sat\",\"bg_flowing\",\"bg_rgb\",\"main_power\"]");
  String v[10];
  int n = parseResult(reply, v, 10);
  if (n < 7) {
    server.send(200, "application/json", "{\"ok\":false}");
    return;
  }
  String j = "{\"ok\":true";
  // У этой лампы «power» показывает «включено», если горит хоть один из двух светов (основной или подсветка).
  // Состояние именно основного света отдаёт «main_power»; если лампа его не вернула, берём «power».
  String mainPower = (n >= 10 && v[9].length() > 0) ? v[9] : v[0];
  j += ",\"power\":\"" + mainPower + "\"";
  j += ",\"bright\":" + String(v[1].toInt());
  j += ",\"ct\":" + String(v[2].toInt());
  j += ",\"bg_power\":\"" + v[3] + "\"";
  j += ",\"bg_bright\":" + String(v[4].toInt());
  j += ",\"hue\":" + String(v[5].toInt());
  j += ",\"sat\":" + String(v[6].toInt());
  j += ",\"bg_flow\":\"" + (n >= 8 ? v[7] : String("")) + "\"";  // "1" — идёт эффект, "0" — нет
  j += ",\"rgb\":" + String(n >= 9 ? v[8].toInt() : 0);             // текущий цвет подсветки, нужен для фиксации
  j += ",\"effect\":\"" + bgEffect + "\"";                          // какой эффект запускали с этой платы
  j += ",\"flow_ms\":" + String(bgFlowMs);
  j += ",\"fhue\":" + String(bgFlowHue);
  j += ",\"fsat\":" + String(bgFlowSat);
  j += ",\"swing\":" + String(bgFlowSwing);
  int32_t sleepLeft = (bgEffect == "sleep") ? (int32_t)(sleepEndMs - millis()) / 1000 : 0;  // секунд до выключения
  j += ",\"sleep_left\":" + String(sleepLeft > 0 ? sleepLeft : 0);
  j += "}";
  server.send(200, "application/json", j);
}

// Цвет из тона (0–359) и насыщенности (0–100) в число 0xRRGGBB для цветового потока лампы
unsigned long hsvToRgb(int hue, int sat) {
  float c = sat / 100.0f;
  float x = c * (1.0f - fabsf(fmodf(hue / 60.0f, 2.0f) - 1.0f));
  float m = 1.0f - c;
  float r = 0, g = 0, b = 0;
  if (hue < 60)       { r = c; g = x; }
  else if (hue < 120) { r = x; g = c; }
  else if (hue < 180) { g = c; b = x; }
  else if (hue < 240) { g = x; b = c; }
  else if (hue < 300) { r = x; b = c; }
  else                { r = c; b = x; }
  return ((unsigned long)lroundf((r + m) * 255) << 16) |
         ((unsigned long)lroundf((g + m) * 255) << 8) |
          (unsigned long)lroundf((b + m) * 255);
}

// Экранирует кавычки и слэши, чтобы имя сети не сломало JSON
String jsonEscape(const String &in) {
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c >= 32) out += c;
  }
  return out;
}

// Сведения о плате и её загруженности для блока System / Control
void handleSystem() {
  float temp = temperatureRead();
  String j = "{";
  j += "\"chip\":\"" + jsonEscape(ESP.getChipModel()) + "\"";
  j += ",\"cores\":" + String(ESP.getChipCores());
  j += ",\"cpu_mhz\":" + String(ESP.getCpuFreqMHz());
  j += ",\"heap_total\":" + String(ESP.getHeapSize());
  j += ",\"heap_free\":" + String(ESP.getFreeHeap());
  j += ",\"heap_min\":" + String(ESP.getMinFreeHeap());
  j += ",\"sketch\":" + String(ESP.getSketchSize());
  j += ",\"sketch_free\":" + String(ESP.getFreeSketchSpace());  // размер раздела под прошивку
  j += ",\"uptime\":" + String(millis() / 1000);
  j += ",\"busy\":" + String(busyPct);
  j += ",\"temp\":" + (isnan(temp) ? String("null") : String(temp, 1));
  j += ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"";
  j += ",\"rssi\":" + String(WiFi.RSSI());
  j += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  j += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  j += "}";
  server.send(200, "application/json", j);
}

// Сцены хранятся на плате одной строкой JSON; страница сама собирает и разбирает список
void handleScenesGet() {
  server.send(200, "application/json", prefs.getString("scenes", "[]"));
}

void handleScenesPost() {
  String body = server.arg("plain");
  if (body.length() > 3000 || !body.startsWith("[") || !body.endsWith("]")) {
    server.send(400, "application/json", "{\"ok\":false}");
    return;
  }
  prefs.putString("scenes", body);
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleReboot() {
  server.send(200, "application/json", "{\"ok\":true}");
  delay(300);  // даём ответу уйти в браузер
  ESP.restart();
}

// ---------- Эффекты из таблиц ----------
// Шаг эффекта: t — длительность перехода в сотых долях от базового времени (100 = всё время),
// rgb — цвет (0 означает «цвет с ползунков»), b — яркость в процентах от выбранной.
struct FlowStep { uint16_t t; uint32_t rgb; uint8_t b; };

struct FlowDef {
  const char *name;
  const FlowStep *steps;
  uint8_t n;
  bool once;       // идёт один раз за заданное время (закат, рассвет), а не по кругу
  bool offAtEnd;   // после окончания подсветка гаснет
  bool useColor;   // цвет берётся с ползунков (hue, sat)
  bool swing;      // у шагов через один яркость уменьшается на «размах» (палитры переливания)
  uint16_t minMs;  // быстрее этого шаги не делаем: слишком частые вспышки утомляют глаза
};

// Свеча: один тёплый цвет, спокойные небольшие колебания яркости
const FlowStep FLOW_CANDLE[] = {
  {80, 0xFF9329, 90}, {60, 0xFF8A1F, 70}, {110, 0xFF9A30, 95}, {70, 0xFF8C22, 75},
  {100, 0xFF9529, 100}, {65, 0xFF871C, 65}, {90, 0xFF9A33, 88}, {75, 0xFF8F25, 78}};

// Закат: из тёплого белого в красный и затухание, весь путь занимает выбранное время
const FlowStep FLOW_SUNSET[] = {
  {15, 0xFFE9C8, 100}, {20, 0xFFB347, 85}, {20, 0xFF7A18, 60},
  {20, 0xE8431A, 35}, {15, 0x8B1A1A, 15}, {10, 0x300606, 2}};

// Рассвет: обратный путь, от тёмно-красного к яркому тёплому белому
const FlowStep FLOW_SUNRISE[] = {
  {10, 0x300606, 2}, {15, 0x8B1A1A, 15}, {20, 0xE8431A, 35},
  {20, 0xFF7A18, 60}, {20, 0xFFB347, 85}, {15, 0xFFE9C8, 100}};

// Гроза: тёмно-синий фон и редкие короткие белые вспышки
const FlowStep FLOW_STORM[] = {
  {120, 0x0B1238, 20}, {3, 0xFFFFFF, 100}, {6, 0x0B1238, 15}, {3, 0xE0E8FF, 90},
  {250, 0x0A0F2E, 12}, {4, 0xFFFFFF, 100}, {10, 0x0B1238, 18}, {150, 0x101C4A, 25}};

// Океан: зелёный, бирюзовый, синий и фиолетовый медленно перетекают друг в друга
const FlowStep FLOW_OCEAN[] = {
  {100, 0x00A8A8, 70}, {120, 0x0066CC, 60}, {100, 0x2A00B8, 55},
  {120, 0x00C878, 70}, {100, 0x00B4D8, 65}, {120, 0x5A1AB8, 55}};

// Лава: тёмно-красный, малиновый и оранжевый, очень медленно, с редкими яркими вспышками
const FlowStep FLOW_LAVA[] = {
  {200, 0x8B0000, 35}, {150, 0xC81E00, 55}, {120, 0xFF4500, 80}, {80, 0xFF7A00, 100},
  {160, 0xC81E00, 50}, {220, 0x600000, 25}, {140, 0xE02800, 65}};

// Сердцебиение: два быстрых удара и пауза, цвет с ползунков
const FlowStep FLOW_HEART[] = {
  {12, 0, 100}, {14, 0, 30}, {12, 0, 85}, {30, 0, 12}, {110, 0, 10}};

// Гирлянда: красный, зелёный, золотой и синий сменяются резко (короткий переход, потом выдержка)
const FlowStep FLOW_GARLAND[] = {
  {2, 0xFF0000, 100}, {100, 0xFF0000, 100}, {2, 0x00CC00, 100}, {100, 0x00CC00, 100},
  {2, 0xFFCC00, 100}, {100, 0xFFCC00, 100}, {2, 0x0000FF, 100}, {100, 0x0000FF, 100}};

// Мигалка: два красных вспыхивания, потом два синих
const FlowStep FLOW_POLICE[] = {
  {5, 0xFF0000, 100}, {5, 0xFF0000, 1}, {5, 0xFF0000, 100}, {5, 0xFF0000, 1},
  {5, 0x0000FF, 100}, {5, 0x0000FF, 1}, {5, 0x0000FF, 100}, {5, 0x0000FF, 1}};

// Палитры переливания: шесть цветов по кругу
const FlowStep FLOW_WARM[] = {
  {100, 0xFF3B00, 100}, {100, 0xFF6A00, 100}, {100, 0xFFA000, 100},
  {100, 0xFFC833, 100}, {100, 0xFF2D55, 100}, {100, 0xFF8A3D, 100}};
const FlowStep FLOW_COLD[] = {
  {100, 0x0066FF, 100}, {100, 0x00B4FF, 100}, {100, 0x00E0D0, 100},
  {100, 0x6A5CFF, 100}, {100, 0x00A0A0, 100}, {100, 0x3AA0FF, 100}};
const FlowStep FLOW_NEON[] = {
  {100, 0xFF00FF, 100}, {100, 0x00FFFF, 100}, {100, 0x39FF14, 100},
  {100, 0xFF1493, 100}, {100, 0x1F51FF, 100}, {100, 0xFFFF00, 100}};
const FlowStep FLOW_PASTEL[] = {
  {100, 0xFFB3BA, 100}, {100, 0xFFDFBA, 100}, {100, 0xFFFFBA, 100},
  {100, 0xBAFFC9, 100}, {100, 0xBAE1FF, 100}, {100, 0xE0BBFF, 100}};

#define FLOW_N(a) ((uint8_t)(sizeof(a) / sizeof((a)[0])))
const FlowDef FLOW_DEFS[] = {
  //  название   шаги            число шагов          один раз  погаснуть  цвет   размах  минимум, мс
  {"candle",  FLOW_CANDLE,  FLOW_N(FLOW_CANDLE),  false, false, false, false, 50},
  {"sunset",  FLOW_SUNSET,  FLOW_N(FLOW_SUNSET),  true,  true,  false, false, 50},
  {"sunrise", FLOW_SUNRISE, FLOW_N(FLOW_SUNRISE), true,  false, false, false, 50},
  {"storm",   FLOW_STORM,   FLOW_N(FLOW_STORM),   false, false, false, false, 80},
  {"ocean",   FLOW_OCEAN,   FLOW_N(FLOW_OCEAN),   false, false, false, false, 50},
  {"lava",    FLOW_LAVA,    FLOW_N(FLOW_LAVA),    false, false, false, false, 50},
  {"heart",   FLOW_HEART,   FLOW_N(FLOW_HEART),   false, false, true,  false, 50},
  {"garland", FLOW_GARLAND, FLOW_N(FLOW_GARLAND), false, false, false, false, 50},
  {"police",  FLOW_POLICE,  FLOW_N(FLOW_POLICE),  false, false, false, false, 150},
  {"warm",    FLOW_WARM,    FLOW_N(FLOW_WARM),    false, false, false, true,  50},
  {"cold",    FLOW_COLD,    FLOW_N(FLOW_COLD),    false, false, false, true,  50},
  {"neon",    FLOW_NEON,    FLOW_N(FLOW_NEON),    false, false, false, true,  50},
  {"pastel",  FLOW_PASTEL,  FLOW_N(FLOW_PASTEL),  false, false, false, true,  50},
};

const FlowDef *findFlow(const String &name) {
  for (const FlowDef &d : FLOW_DEFS) {
    if (name == d.name) return &d;
  }
  return nullptr;
}

// Собирает строку-цепочку для цветового потока лампы
String buildFlow(const FlowDef &d, long baseMs, int br, unsigned long userRgb, int swing) {
  String f;
  for (int i = 0; i < d.n; i++) {
    if (i) f += ",";
    long t = baseMs * d.steps[i].t / 100;
    if (t < d.minMs) t = d.minMs;
    unsigned long rgb = d.steps[i].rgb ? d.steps[i].rgb : userRgb;
    int b = br * d.steps[i].b / 100;
    if (d.swing && (i % 2)) b = b * (100 - swing) / 100;  // размах: каждый второй цвет тусклее
    if (b < 1) b = 1;
    // длительность, режим 1 (RGB), цвет, яркость
    f += String(t) + ",1," + String(rgb) + "," + String(b);
  }
  return f;
}

void handleSet() {
  bool allOk = true;
  bool lampSilent = false;  // лампа не ответила совсем: остальные команды пробовать нет смысла
  uint32_t nextGap = 1000;  // пауза перед следующей командой, мс
  auto run = [&](const String &method, const String &params) {
    if (lampSilent) { allOk = false; return; }
    String reply = sendCmd(method, params, nextGap);
    nextGap = 1000;
    if (reply.length() == 0) lampSilent = true;
    if (!replyOk(reply)) allOk = false;
  };

  if (server.hasArg("power")) {
    run("set_power", String("[\"") + (server.arg("power") == "on" ? "on" : "off") + "\",\"smooth\",300]");
    // Лампа гасит оба света сразу, а страница тут же просит вернуть подсветку: не ждём лишнего,
    // чтобы подсветка была выключена как можно меньше
    if (server.arg("power") != "on" &&
        (server.hasArg("bg_power") || server.hasArg("bg_flow") || server.hasArg("bg_sleep"))) {
      nextGap = 250;
    }
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
  if (server.hasArg("bg_power") || server.hasArg("bg_flow") || server.hasArg("hue") || server.hasArg("bg_bright") ||
      server.hasArg("bg_sleep")) {
    sleepEndMs = 0;
    bgEffect = "off";  // любая из этих команд останавливает эффект на лампе; запуск эффекта ниже запишет его заново
  }
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
  // Эффекты подсветки: лампа сама крутит цвета, плата после запуска ей не нужна.
  // Запускаем сценой «cf», как и обычное включение подсветки: она заодно включает подсветку.
  //   bg_flow=on      — переливание по кругу радуги
  //   bg_flow=wave    — волна: яркость плавно растёт и падает в выбранном цвете (hue, sat)
  //   bg_flow=fire    — костёр: тёплые оранжево-красные цвета с неровным мерцанием
  //   bg_flow=<имя>   — эффекты из таблицы выше (свеча, закат, рассвет, гроза, океан, лава,
  //                     сердцебиение, гирлянда, мигалка)
  //   bg_flow=off     — остановить эффект
  if (server.hasArg("bg_flow")) {
    String mode = server.arg("bg_flow");
    const FlowDef *def = findFlow(mode);
    if (mode == "on" || mode == "wave" || mode == "fire" || def) {
      long flowMs = server.arg("flow_ms").toInt();
      int stepMs = constrain(flowMs, 500, 30000);            // время одного шага
      long totalMs = constrain(flowMs, 10000L, 3600000L);    // общая длительность заката и рассвета
      int br = server.hasArg("bg_bright") ? constrain(server.arg("bg_bright").toInt(), 1, 100) : 100;
      int hue = server.hasArg("hue") ? constrain(server.arg("hue").toInt(), 0, 359) : 0;
      int sat = server.hasArg("sat") ? constrain(server.arg("sat").toInt(), 0, 100) : 100;
      int swing = server.hasArg("swing") ? constrain(server.arg("swing").toInt(), 0, 90) : 0;
      String flow = "";
      int count = 0;   // 0 — крутить бесконечно
      int action = 1;  // что делать после остановки: 1 — остаться на текущем цвете, 2 — погасить
      if (mode == "on") {
        const unsigned long colors[] = {0xFF0000, 0xFFFF00, 0x00FF00, 0x00FFFF, 0x0000FF, 0xFF00FF};
        for (int i = 0; i < 6; i++) {
          if (i) flow += ",";
          // длительность, режим 1 (RGB), цвет, яркость (у каждого второго цвета ниже на размах)
          int bb = (i % 2) ? br * (100 - swing) / 100 : br;
          flow += String(stepMs) + ",1," + String(colors[i]) + "," + String(max(1, bb));
        }
      } else if (mode == "fire") {
        // Цвета и яркость идут вразнобой, длительности разные: так пламя выглядит живым.
        // Длительность шага — доля от выбранного времени, чтобы мерцание было быстрее переливания.
        const unsigned long fc[] = {0xFF3C00, 0xFF8C00, 0xFF2800, 0xFFB400, 0xFF5000,
                                    0xFF6A00, 0xFFD23C, 0xFF3200, 0xFF9600, 0xFF4600};
        const int fb[] = {100, 45, 80, 30, 65, 95, 40, 75, 55, 90};      // яркость в процентах от выбранной
        const int ft[] = {15, 30, 20, 40, 12, 25, 35, 18, 28, 22};       // длительность в сотых долях шага
        for (int i = 0; i < 10; i++) {
          if (i) flow += ",";
          int t = max(50, stepMs * ft[i] / 100);
          flow += String(t) + ",1," + String(fc[i]) + "," + String(max(1, br * fb[i] / 100));
        }
      } else if (mode == "wave") {
        int lo = max(1, br * 15 / 100);  // нижняя точка волны
        String rgb = String(hsvToRgb(hue, sat));
        flow = String(stepMs) + ",1," + rgb + "," + String(lo) + "," +
               String(stepMs) + ",1," + rgb + "," + String(br);
      } else {
        flow = buildFlow(*def, def->once ? totalMs : (long)stepMs, br, hsvToRgb(hue, sat), swing);
        if (def->once) {
          count = def->n;  // один проход: столько шагов, сколько в таблице
          action = def->offAtEnd ? 2 : 1;
        }
      }
      run("bg_set_scene", "[\"cf\"," + String(count) + "," + String(action) + ",\"" + flow + "\"]");
      bgDone = true;
      bgEffect = mode;
      bgFlowMs = (def && def->once) ? (int)totalMs : stepMs;
      if (mode == "wave" || (def && def->useColor)) {
        bgFlowHue = hue;
        bgFlowSat = sat;
      }
      bgFlowSwing = swing;
    } else if (server.hasArg("hue")) {
      // Выход из эффекта сразу в нужный цвет: одна сцена вместо «остановить» + «задать цвет».
      // Так цвет меняется на секунду раньше: лампе нельзя слать команды чаще раза в секунду.
      int hue = constrain(server.arg("hue").toInt(), 0, 359);
      int sat = server.hasArg("sat") ? constrain(server.arg("sat").toInt(), 0, 100) : 100;
      int br  = server.hasArg("bg_bright") ? constrain(server.arg("bg_bright").toInt(), 1, 100) : 100;
      run("bg_set_scene", "[\"hsv\"," + String(hue) + "," + String(sat) + "," + String(br) + "]");
      bgDone = true;
    } else {
      run("bg_stop_cf", "[]");
    }
  }
  // Таймер сна: подсветка за N минут плавно гаснет. Считает сама лампа: цепочка из двух шагов
  // (сначала текущий цвет и яркость, потом медленное затухание до минимума) и действие «выключить» в конце.
  if (server.hasArg("bg_sleep")) {
    int minutes = constrain(server.arg("bg_sleep").toInt(), 1, 240);
    int hue = server.hasArg("hue") ? constrain(server.arg("hue").toInt(), 0, 359) : 0;
    int sat = server.hasArg("sat") ? constrain(server.arg("sat").toInt(), 0, 100) : 100;
    int br  = server.hasArg("bg_bright") ? constrain(server.arg("bg_bright").toInt(), 1, 100) : 100;
    long ms = minutes * 60000L;
    String rgb = String(hsvToRgb(hue, sat));
    String flow = String("50,1,") + rgb + "," + String(br) + "," + String(ms) + ",1," + rgb + ",1";
    run("bg_set_scene", "[\"cf\",2,2,\"" + flow + "\"]");  // 2 шага, после них выключить
    bgDone = true;
    bgEffect = "sleep";
    sleepEndMs = millis() + (uint32_t)ms;
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

  prefs.begin("lamp", false);  // память для сцен
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
  server.on("/api/system", HTTP_GET, handleSystem);
  server.on("/api/scenes", HTTP_GET, handleScenesGet);
  server.on("/api/scenes", HTTP_POST, handleScenesPost);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.begin();
}

void loop() {
  uint32_t t0 = micros();
  server.handleClient();
  busyUs += micros() - t0;
  if (millis() - busyWindowStart >= 10000) {
    uint32_t pct = busyUs / ((millis() - busyWindowStart) * 10UL);  // микросекунды в проценты окна
    busyPct = pct > 100 ? 100 : pct;
    busyUs = 0;
    busyWindowStart = millis();
  }

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