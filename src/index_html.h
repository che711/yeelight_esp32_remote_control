#pragma once

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Лампа</title>
<style>
  :root { color-scheme: dark; --bg:#14161a; --card:#1e2127; --line:#2c3038; --text:#e8eaed; --dim:#8b919a; --accent:#f5a623; }
  * { box-sizing: border-box; }
  body { margin:0; padding:16px; background:var(--bg); color:var(--text); font:16px/1.4 system-ui,sans-serif; }
  main { max-width:480px; margin:0 auto; }
  h1 { font-size:20px; margin:4px 0 4px; }
  #status { font-size:13px; color:var(--dim); margin-bottom:14px; }
  #status.bad { color:#ff6b6b; }
  details.card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px 16px; margin-bottom:14px; }
  details.card > summary { font-size:15px; color:var(--dim); font-weight:600; cursor:pointer; list-style:none; user-select:none; }
  details.card > summary::-webkit-details-marker { display:none; }
  details.card > summary::before { content:'▾'; display:inline-block; width:1.3em; }
  details.card:not([open]) > summary::before { content:'▸'; }
  details.card[open] > summary { margin-bottom:10px; }
  .row { display:flex; justify-content:space-between; align-items:center; margin:12px 0 6px; }
  .row b { color:var(--accent); font-weight:600; }
  input[type=range] { width:100%; height:28px; accent-color:var(--accent); }
  #hue { -webkit-appearance:none; appearance:none; height:14px; border-radius:7px; margin:6px 0;
         background:linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00); }
  #hue::-webkit-slider-thumb { -webkit-appearance:none; width:22px; height:22px; border-radius:50%; background:#fff; border:2px solid #000; }
  #hue::-moz-range-thumb { width:22px; height:22px; border-radius:50%; background:#fff; border:2px solid #000; }
  .sw { width:50px; height:28px; accent-color:var(--accent); transform:scale(1.2); }
  .sw:disabled { opacity:.4; }
  #swatch { width:100%; height:10px; border-radius:5px; margin-top:10px; }
  #white { margin-top:10px; padding:8px 18px; border:1px solid var(--line); border-radius:8px;
           background:#2a2e36; color:var(--text); font:inherit; cursor:pointer; }
  #white:active { background:var(--accent); color:#000; }
  .seg { display:flex; gap:6px; flex-wrap:wrap; margin:4px 0 6px; }
  .seg button { flex:1 1 40%; padding:9px 6px; border:1px solid var(--line); border-radius:8px;
                background:#2a2e36; color:var(--text); font:inherit; cursor:pointer; }
  .seg button:active { background:var(--accent); border-color:var(--accent); color:#000; }
  .seg button.on { background:var(--accent); border-color:var(--accent); color:#000; font-weight:600; }
  .kv { display:flex; justify-content:space-between; gap:12px; margin:8px 0 2px; font-size:14px; }
  .kv span:first-child { color:var(--dim); }
  .kv span:last-child { text-align:right; word-break:break-all; }
  .bar { height:8px; border-radius:4px; background:#2a2e36; overflow:hidden; margin:2px 0 6px; }
  .bar i { display:block; height:100%; width:0; background:var(--accent); transition:width .4s; }
  .bar i.hot { background:#ff6b6b; }
  #reboot { margin-top:12px; padding:8px 18px; border:1px solid #6b2c2c; border-radius:8px;
            background:#3a2020; color:#ffb4b4; font:inherit; cursor:pointer; }
  #reboot:active { background:#ff6b6b; color:#000; }
  #sys_note { font-size:12px; color:var(--dim); margin-top:8px; }
</style>
</head>
<body>
<main>
  <h1>Лампа Yeelight</h1>
  <div id="status">читаю состояние лампы…</div>

  <details class="card" id="sec_main" open>
    <summary>Основной свет</summary>
    <label class="row"><span>Включён</span><input type="checkbox" id="power" class="sw" autocomplete="off" disabled></label>
    <div class="row"><span>Яркость</span><b id="bright_v"></b></div>
    <input type="range" id="bright" min="1" max="100" autocomplete="off">
    <div class="row"><span>Цветовая температура</span><b id="ct_v"></b></div>
    <input type="range" id="ct" min="2700" max="6500" step="100" autocomplete="off">
  </details>

  <details class="card" id="sec_bg" open>
    <summary>Подсветка сзади</summary>
    <label class="row"><span>Включена</span><input type="checkbox" id="bg_power" class="sw" autocomplete="off" disabled></label>
    <div class="row"><span>Яркость</span><b id="bg_bright_v"></b></div>
    <input type="range" id="bg_bright" min="1" max="100" autocomplete="off">
    <div class="row"><span>Эффект</span></div>
    <div class="seg" id="bg_effect">
      <button type="button" data-v="off" class="on">Нет</button>
      <button type="button" data-v="rainbow">Переливание</button>
      <button type="button" data-v="wave">Волна</button>
      <button type="button" data-v="fire">Костёр</button>
      <button type="button" id="bg_lock">Зафиксировать цвет</button>
    </div>
    <div class="row"><span>Время одного шага</span><b id="flow_s_v"></b></div>
    <input type="range" id="flow_s" min="1" max="20" value="3" autocomplete="off">
    <div class="row"><span>Цвет</span><b id="hue_v"></b></div>
    <input type="range" id="hue" min="0" max="359" autocomplete="off">
    <div class="row"><span>Насыщенность</span><b id="sat_v"></b></div>
    <input type="range" id="sat" min="0" max="100" autocomplete="off">
    <button id="white" type="button">Белый</button>
    <div id="swatch"></div>
  </details>

  <details class="card" id="sec_sys" open>
    <summary>System</summary>
    <div class="kv"><span>Плата</span><span id="sys_chip">—</span></div>
    <div class="kv"><span>Время работы</span><span id="sys_uptime">—</span></div>
    <div class="kv"><span>Память (куча)</span><span id="sys_heap">—</span></div>
    <div class="bar"><i id="sys_heap_bar"></i></div>
    <div class="kv"><span>Минимум свободной памяти</span><span id="sys_heap_min">—</span></div>
    <div class="kv"><span>Прошивка</span><span id="sys_sketch">—</span></div>
    <div class="bar"><i id="sys_sketch_bar"></i></div>
    <div class="kv"><span>Занятость сервера</span><span id="sys_busy">—</span></div>
    <div class="bar"><i id="sys_busy_bar"></i></div>
    <div class="kv"><span>Температура чипа</span><span id="sys_temp">—</span></div>
    <div class="kv"><span>Wi-Fi</span><span id="sys_wifi">—</span></div>
    <div class="kv"><span>IP-адрес</span><span id="sys_ip">—</span></div>
    <div class="kv"><span>MAC-адрес</span><span id="sys_mac">—</span></div>
    <button id="reboot" type="button">Перезагрузить плату</button>
    <div id="sys_note"></div>
  </details>
</main>

<script>
const $ = id => document.getElementById(id);
const suffix = { bright:'%', ct:' K', bg_bright:'%', hue:'°', sat:'%', flow_s:' с' };
const FLOW_GRADIENT = 'linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00)';
const FIRE_GRADIENT = 'linear-gradient(90deg,#c21800,#ff6a00,#ffb400,#ff3c00,#ff8c00,#ff2800)';
let lastAction = 0;
let inflight = false;
let queued = {};
let urgent = false;         // команда от кнопки: отправляем сразу, без ожидания паузы между запросами
let stateLoaded = false;    // состояние лампы прочитано хотя бы раз

// ---------- Сворачиваемые блоки ----------

document.querySelectorAll('details.card').forEach(d => {
  try { if (localStorage.getItem('open_' + d.id) === '0') d.open = false; } catch (e) {}
  d.addEventListener('toggle', () => {
    try { localStorage.setItem('open_' + d.id, d.open ? '1' : '0'); } catch (e) {}
    if (d.id === 'sec_sys' && d.open) loadSystem();
  });
});

// ---------- Лампа ----------

function setStatus(ok) {
  const s = $('status');
  s.textContent = ok ? 'лампа на связи' : 'лампа не отвечает';
  s.className = ok ? '' : 'bad';
}

function effect() {  // off, rainbow, wave или fire
  const b = document.querySelector('#bg_effect button.on');
  return b ? b.dataset.v : 'off';
}
function setEffect(v) {
  document.querySelectorAll('#bg_effect button[data-v]').forEach(b => b.classList.toggle('on', b.dataset.v === v));
}
function flowOn() { return effect() !== 'off'; }

// Параметры запуска эффекта: цвета лампа крутит сама.
// Переливанию и костру нужны скорость и яркость, волне ещё и цвет.
function flowParams() {
  const modes = { rainbow: 'on', wave: 'wave', fire: 'fire' };
  const p = { bg_flow: modes[effect()], flow_ms: $('flow_s').value * 1000, bg_bright: $('bg_bright').value };
  if (effect() === 'wave') { p.hue = $('hue').value; p.sat = $('sat').value; }
  return p;
}

function updateSwatch() {
  const col = hsvCss(+$('hue').value, +$('sat').value);
  const e = effect();
  $('swatch').style.background = e === 'rainbow' ? FLOW_GRADIENT
    : e === 'fire' ? FIRE_GRADIENT
    : e === 'wave' ? 'linear-gradient(90deg,#000,' + col + ',#000)' : col;
}

function updateLabel(id) {
  $(id + '_v').textContent = $(id).value + suffix[id];
  if (id === 'hue' || id === 'sat') updateSwatch();
}

// Обратное преобразование: число 0xRRGGBB от лампы в тон и насыщенность для ползунков
function rgbToHs(n) {
  const r = (n >> 16 & 255) / 255, g = (n >> 8 & 255) / 255, b = (n & 255) / 255;
  const mx = Math.max(r, g, b), d = mx - Math.min(r, g, b);
  let h = 0;
  if (d) {
    if (mx === r)      h = 60 * (((g - b) / d) % 6);
    else if (mx === g) h = 60 * ((b - r) / d + 2);
    else               h = 60 * ((r - g) / d + 4);
  }
  if (h < 0) h += 360;
  return { hue: Math.round(h) % 360, sat: mx ? Math.round(d / mx * 100) : 0 };
}

// HSV (яркость 100%) в цвет для полоски-образца; при насыщенности 0 получается белый
function hsvCss(h, s) {
  const c = s / 100;
  const x = c * (1 - Math.abs((h / 60) % 2 - 1));
  const m = 1 - c;
  let r = 0, g = 0, b = 0;
  if (h < 60)       { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else              { r = c; b = x; }
  const to255 = v => Math.round((v + m) * 255);
  return 'rgb(' + to255(r) + ',' + to255(g) + ',' + to255(b) + ')';
}

// Лампа принимает около 60 команд в минуту, при превышении она замолкает на минуту и дольше.
// Поэтому запросы от ползунков уходят не чаще раза в 1,2 с, а при движении копится последнее значение.
// Нажатия кнопок и переключателей отправляются сразу (urgent), они редкие и ждать им незачем.
const MIN_GAP_MS = 1200;
let lastSent = 0;
let timer = null;
let syncAfterSend = false;  // после фиксации цвета читаем у лампы, на каком цвете она остановилась

function queue(params, now) {
  Object.assign(queued, params);
  lastAction = Date.now();
  if (now) urgent = true;
  schedule();
}

function schedule() {
  if (inflight) return;  // после ответа flush() вызовет schedule() снова
  if (timer) {
    if (!urgent) return;
    clearTimeout(timer);
    timer = null;
  }
  const wait = urgent ? 0 : Math.max(0, MIN_GAP_MS - (Date.now() - lastSent));
  urgent = false;
  timer = setTimeout(() => { timer = null; flush(); }, wait);
}

// fetch с ограничением по времени: без него зависший запрос навсегда блокирует очередь команд
function fetchJson(url, ms) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), ms);
  return fetch(url, { signal: ctrl.signal })
    .then(r => r.json())
    .finally(() => clearTimeout(t));
}

function flush() {
  if (!Object.keys(queued).length) return;
  const params = queued;
  queued = {};
  inflight = true;
  lastSent = Date.now();
  fetchJson('/api/set?' + new URLSearchParams(params), 30000)
    .then(j => setStatus(j.ok))
    .catch(() => setStatus(false))
    .finally(() => {
      inflight = false;
      if (Object.keys(queued).length) schedule();
      else if (syncAfterSend) { syncAfterSend = false; setTimeout(() => refresh(true), MIN_GAP_MS); }
    });
}

['bright', 'ct'].forEach(id => {
  $(id).addEventListener('input', () => {
    updateLabel(id);
    queue({ [id]: $(id).value });
  });
});

// Кнопки эффектов: нажатие сразу запускает эффект или останавливает его
$('bg_effect').addEventListener('click', e => {
  const b = e.target.closest('button[data-v]');
  if (!b || b.dataset.v === effect()) return;
  setEffect(b.dataset.v);
  updateSwatch();
  queue(flowOn() ? flowParams() : { bg_flow: 'off' }, true);
});

// Пока эффект включён, яркость меняем, перезапуская его с новой яркостью
$('bg_bright').addEventListener('input', () => {
  updateLabel('bg_bright');
  queue(flowOn() ? flowParams() : { bg_bright: $('bg_bright').value });
});

// Остановить эффект на том цвете, который сейчас горит. Кнопка остаётся на месте и работает всегда
$('bg_lock').addEventListener('click', () => {
  if (flowOn()) {
    setEffect('off');
    updateSwatch();
    syncAfterSend = true;
    queue({ bg_flow: 'off' }, true);
  } else {
    refresh(true);  // эффекта нет: просто считываем цвет с лампы в ползунки
  }
});

$('flow_s').addEventListener('input', () => {
  updateLabel('flow_s');
  if (flowOn()) queue(flowParams());
});

// Ручной выбор цвета выключает переливание и костёр; волна при этом просто меняет цвет.
// Яркость отправляем вместе с цветом: сервер выходит из эффекта одной командой.
function leaveRainbow() {
  if (effect() === 'off' || effect() === 'wave') return {};
  setEffect('off');
  return { bg_flow: 'off', bg_bright: $('bg_bright').value };
}

['hue', 'sat'].forEach(id => {
  $(id).addEventListener('input', () => {
    const stop = leaveRainbow();
    // При насыщенности 0 цвет всегда белый, поэтому ползунок цвета ничего бы не менял.
    // Если после «Белого» двигают цвет, возвращаем насыщенность на максимум.
    if (id === 'hue' && +$('sat').value === 0) {
      $('sat').value = 100;
      updateLabel('sat');
    }
    updateLabel(id);
    queue(effect() === 'wave' ? flowParams() : Object.assign({ hue: $('hue').value, sat: $('sat').value }, stop));
  });
});

$('white').addEventListener('click', () => {
  const stop = leaveRainbow();
  $('sat').value = 0;
  updateLabel('sat');
  queue(effect() === 'wave' ? flowParams() : Object.assign({ hue: $('hue').value, sat: 0 }, stop), true);
});

$('power').addEventListener('change', () => {
  queue({ power: $('power').checked ? 'on' : 'off' }, true);
});

$('bg_power').addEventListener('change', () => {
  const on = $('bg_power').checked;
  if (!on) {
    setEffect('off');
    updateSwatch();
    queue({ bg_power: 'off' }, true);
  } else if (flowOn()) {
    // Если эффект выбран, подсветка включается сразу им
    queue(flowParams(), true);
  } else {
    // Включая подсветку, сразу задаём цвет: так она загорается цветным светом, а не в «последнем» режиме лампы
    queue({ bg_power: 'on', hue: $('hue').value, sat: $('sat').value, bg_bright: $('bg_bright').value }, true);
  }
});

// ---------- Состояние лампы ----------

let refreshing = false;
const EFFECT_FROM_BOARD = { on: 'rainbow', wave: 'wave', fire: 'fire' };

function refresh(afterLock) {
  if (inflight || timer || refreshing) return;
  if (!afterLock && Date.now() - lastAction < 5000) return;  // не мешаем, пока двигаете ползунок
  refreshing = true;
  const actionAtStart = lastAction;
  fetchJson('/api/state', 10000)
    .then(j => {
      setStatus(j.ok);
      if (!j.ok) return;
      if (!afterLock && lastAction !== actionAtStart) return;  // пока шёл запрос, трогали ползунки: ответ устарел
      $('power').checked = (j.power === 'on');
      $('bg_power').checked = (j.bg_power === 'on');
      if (!stateLoaded) {  // переключатели ждут первого ответа лампы, чтобы не показывать выдуманное состояние
        stateLoaded = true;
        $('power').disabled = false;
        $('bg_power').disabled = false;
      }
      // Лампа сообщает, идёт ли эффект; какой именно, помнит плата. Если плата перезагружалась, считаем переливанием
      let flowing = false;
      if (j.bg_flow === '0') {
        setEffect('off');
      } else if (j.bg_flow === '1') {
        flowing = true;
        const known = EFFECT_FROM_BOARD[j.effect];
        setEffect(known || 'rainbow');
        if (known && j.flow_ms) {
          $('flow_s').value = Math.min(20, Math.max(1, Math.round(j.flow_ms / 1000)));
          updateLabel('flow_s');
        }
      }
      ['bright', 'ct', 'bg_bright', 'hue', 'sat'].forEach(id => {
        // Во время эффекта цвет лампы меняется, ползунки цвета не трогаем
        const skip = flowing && (id === 'hue' || id === 'sat');
        if (!skip && j[id] !== undefined) $(id).value = j[id];
        updateLabel(id);
      });
      if (flowing && effect() === 'wave' && j.effect === 'wave') {
        $('hue').value = j.fhue;
        $('sat').value = j.fsat;
        updateLabel('hue');
        updateLabel('sat');
      }
      // После фиксации ставим ползунки цвета на тот цвет, где остановилась лампа
      if (afterLock === true && j.rgb > 0) {
        const hs = rgbToHs(j.rgb);
        $('hue').value = hs.hue;
        $('sat').value = hs.sat;
        updateLabel('hue');
        updateLabel('sat');
      }
      updateSwatch();
    })
    .catch(() => setStatus(false))
    .finally(() => { refreshing = false; });
}

// ---------- System ----------

const kb = n => (n / 1024).toFixed(0) + ' КБ';

function setBar(id, pct) {
  const el = $(id);
  el.style.width = Math.max(0, Math.min(100, pct)) + '%';
  el.classList.toggle('hot', pct >= 85);
}

function fmtUptime(sec) {
  const d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600), m = Math.floor(sec % 3600 / 60);
  if (d) return d + ' д ' + h + ' ч';
  if (h) return h + ' ч ' + m + ' мин';
  return m + ' мин ' + (sec % 60) + ' с';
}

function wifiQuality(rssi) {
  if (rssi >= -55) return 'отличный';
  if (rssi >= -67) return 'хороший';
  if (rssi >= -75) return 'средний';
  return 'слабый';
}

let sysBusy = false;

function loadSystem() {
  if (sysBusy || !$('sec_sys').open) return;  // свёрнутый блок не опрашиваем
  sysBusy = true;
  fetchJson('/api/system', 8000)
    .then(j => {
      $('sys_chip').textContent = j.chip + ', ' + j.cores + ' яд., ' + j.cpu_mhz + ' МГц';
      $('sys_uptime').textContent = fmtUptime(j.uptime);
      const used = j.heap_total - j.heap_free;
      $('sys_heap').textContent = 'занято ' + kb(used) + ' из ' + kb(j.heap_total);
      setBar('sys_heap_bar', used / j.heap_total * 100);
      $('sys_heap_min').textContent = kb(j.heap_min);
      if (j.sketch_free > 0) {
        $('sys_sketch').textContent = kb(j.sketch) + ' из ' + kb(j.sketch_free);
        setBar('sys_sketch_bar', j.sketch / j.sketch_free * 100);
      } else {
        $('sys_sketch').textContent = kb(j.sketch);
        setBar('sys_sketch_bar', 0);
      }
      $('sys_busy').textContent = j.busy + '%';
      setBar('sys_busy_bar', j.busy);
      $('sys_temp').textContent = j.temp === null ? 'нет данных' : j.temp.toFixed(1) + ' °C';
      $('sys_wifi').textContent = j.ssid + ', ' + j.rssi + ' дБм (' + wifiQuality(j.rssi) + ')';
      $('sys_ip').textContent = j.ip;
      $('sys_mac').textContent = j.mac;
      $('sys_note').textContent = '';
    })
    .catch(() => { $('sys_note').textContent = 'плата не ответила, пробую снова'; })
    .finally(() => { sysBusy = false; });
}

$('reboot').addEventListener('click', () => {
  if (!confirm('Перезагрузить плату? Страница будет недоступна около 10 секунд.')) return;
  fetch('/api/reboot', { method: 'POST' }).catch(() => {});
  $('sys_note').textContent = 'плата перезагружается…';
  setTimeout(() => location.reload(), 10000);
});

['bright', 'ct', 'bg_bright', 'hue', 'sat', 'flow_s'].forEach(updateLabel);
refresh();
setInterval(() => { if (!stateLoaded) refresh(); }, 3000);  // пока лампа не ответила, пробуем снова
setInterval(refresh, 15000);
setTimeout(loadSystem, 3000);  // даём состоянию лампы прочитаться первым
setInterval(loadSystem, 5000);
</script>
</body>
</html>
)HTML";
