#pragma once

// ---- Wi-Fi ----
#define WIFI_SSID "ВАШ_WIFI"
#define WIFI_PASS "ВАШ_ПАРОЛЬ"

// ---- Лампа ----
#define LAMP_IP   "192.168.2.6"
#define LAMP_PORT 55443

// ---- Энкодер EC11 (общий контакт на GND, подтяжки включаются в коде) ----
#define PIN_ENC_A   1
#define PIN_ENC_B   2
#define PIN_ENC_BTN 3

// Сколько переходов даёт один щелчок энкодера: у EC11 обычно 4, у некоторых 2
#define ENC_STEPS_PER_DETENT 4
// Если вращение идёт не в ту сторону — поставьте 1
#define ENC_REVERSE 0

// ---- Встроенный светодиод WS2812 на C6-Zero ----
#define LED_PIN 8
#define LED_LEVEL 10   // яркость индикатора, 0-255

// ---- Шаги регулировки ----
#define STEP_BRIGHT 5     // яркость, %
#define STEP_CT     200   // цветовая температура, K
#define STEP_HUE    10    // оттенок подсветки, градусы

// Долгое нажатие переключает режим
#define LONG_PRESS_MS 600
