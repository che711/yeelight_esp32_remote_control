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
  section { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px 16px; margin-bottom:14px; }
  h2 { font-size:15px; margin:0 0 10px; color:var(--dim); font-weight:600; }
  .row { display:flex; justify-content:space-between; align-items:center; margin:12px 0 6px; }
  .row b { color:var(--accent); font-weight:600; }
  input[type=range] { width:100%; height:28px; accent-color:var(--accent); }
  #hue { -webkit-appearance:none; appearance:none; height:14px; border-radius:7px; margin:6px 0;
         background:linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00); }
  #hue::-webkit-slider-thumb { -webkit-appearance:none; width:22px; height:22px; border-radius:50%; background:#fff; border:2px solid #000; }
  #hue::-moz-range-thumb { width:22px; height:22px; border-radius:50%; background:#fff; border:2px solid #000; }
  .sw { width:50px; height:28px; accent-color:var(--accent); transform:scale(1.2); }
  #swatch { width:100%; height:10px; border-radius:5px; margin-top:10px; }
  #white { margin-top:10px; padding:8px 18px; border:1px solid var(--line); border-radius:8px;
           background:#2a2e36; color:var(--text); font:inherit; cursor:pointer; }
  #white:active { background:var(--accent); color:#000; }
</style>
</head>
<body>
<main>
  <h1>Лампа Yeelight</h1>
  <div id="status">подключаюсь…</div>

  <section>
    <h2>Основной свет</h2>
    <label class="row"><span>Включён</span><input type="checkbox" id="power" class="sw"></label>
    <div class="row"><span>Яркость</span><b id="bright_v"></b></div>
    <input type="range" id="bright" min="1" max="100">
    <div class="row"><span>Цветовая температура</span><b id="ct_v"></b></div>
    <input type="range" id="ct" min="2700" max="6500" step="100">
  </section>

  <section>
    <h2>Подсветка сзади</h2>
    <label class="row"><span>Включена</span><input type="checkbox" id="bg_power" class="sw"></label>
    <div class="row"><span>Яркость</span><b id="bg_bright_v"></b></div>
    <input type="range" id="bg_bright" min="1" max="100">
    <div class="row"><span>Цвет</span><b id="hue_v"></b></div>
    <input type="range" id="hue" min="0" max="359">
    <div class="row"><span>Насыщенность</span><b id="sat_v"></b></div>
    <input type="range" id="sat" min="0" max="100">
    <button id="white" type="button">Белый</button>
    <div id="swatch"></div>
  </section>
</main>

<script>
const $ = id => document.getElementById(id);
const suffix = { bright:'%', ct:' K', bg_bright:'%', hue:'°', sat:'%' };
let lastAction = 0;
let inflight = false;
let queued = {};

function setStatus(ok) {
  const s = $('status');
  s.textContent = ok ? 'лампа на связи' : 'лампа не отвечает';
  s.className = ok ? '' : 'bad';
}

function updateLabel(id) {
  $(id + '_v').textContent = $(id).value + suffix[id];
  if (id === 'hue' || id === 'sat') {
    $('swatch').style.background = hsvCss(+$('hue').value, +$('sat').value);
  }
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
// Поэтому запросы уходят не чаще раза в 1,2 с, а при движении ползунка копится последнее значение.
const MIN_GAP_MS = 1200;
let lastSent = 0;
let timer = null;

function queue(params) {
  Object.assign(queued, params);
  lastAction = Date.now();
  schedule();
}

function schedule() {
  if (inflight || timer) return;
  const wait = Math.max(0, MIN_GAP_MS - (Date.now() - lastSent));
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
    });
}

['bright', 'ct', 'bg_bright'].forEach(id => {
  $(id).addEventListener('input', () => {
    updateLabel(id);
    queue({ [id]: $(id).value });
  });
});

['hue', 'sat'].forEach(id => {
  $(id).addEventListener('input', () => {
    // При насыщенности 0 цвет всегда белый, поэтому ползунок цвета ничего бы не менял.
    // Если после «Белого» двигают цвет, возвращаем насыщенность на максимум.
    if (id === 'hue' && +$('sat').value === 0) {
      $('sat').value = 100;
      updateLabel('sat');
    }
    updateLabel(id);
    queue({ hue: $('hue').value, sat: $('sat').value });
  });
});

$('white').addEventListener('click', () => {
  $('sat').value = 0;
  updateLabel('sat');
  queue({ hue: $('hue').value, sat: 0 });
});

['power', 'bg_power'].forEach(id => {
  $(id).addEventListener('change', () => {
    queue({ [id]: $(id).checked ? 'on' : 'off' });
  });
});

let refreshing = false;

function refresh() {
  if (Date.now() - lastAction < 5000 || inflight || timer || refreshing) return;  // не мешаем, пока двигаете ползунок
  refreshing = true;
  const actionAtStart = lastAction;
  fetchJson('/api/state', 10000)
    .then(j => {
      setStatus(j.ok);
      if (!j.ok) return;
      if (lastAction !== actionAtStart) return;  // пока шёл запрос, трогали ползунки: ответ устарел
      $('power').checked = (j.power === 'on');
      $('bg_power').checked = (j.bg_power === 'on');
      ['bright', 'ct', 'bg_bright', 'hue', 'sat'].forEach(id => {
        if (j[id] !== undefined) $(id).value = j[id];
        updateLabel(id);
      });
    })
    .catch(() => setStatus(false))
    .finally(() => { refreshing = false; });
}

['bright', 'ct', 'bg_bright', 'hue', 'sat'].forEach(updateLabel);
refresh();
setInterval(refresh, 15000);
</script>
</body>
</html>
)HTML";
