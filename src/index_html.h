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
  #swatch { width:100%; height:10px; border-radius:5px; margin-top:6px; }
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
    $('swatch').style.background = 'hsl(' + $('hue').value + ' ' + $('sat').value + '% 50%)';
  }
}

// Отправка: пока идёт запрос, новые значения копятся и уходят одним пакетом
function queue(params) {
  Object.assign(queued, params);
  lastAction = Date.now();
  if (!inflight) flush();
}

function flush() {
  if (!Object.keys(queued).length) return;
  const params = queued;
  queued = {};
  inflight = true;
  fetch('/api/set?' + new URLSearchParams(params))
    .then(r => r.json())
    .then(j => setStatus(j.ok))
    .catch(() => setStatus(false))
    .finally(() => { inflight = false; flush(); });
}

['bright', 'ct', 'bg_bright'].forEach(id => {
  $(id).addEventListener('input', () => {
    updateLabel(id);
    queue({ [id]: $(id).value });
  });
});

['hue', 'sat'].forEach(id => {
  $(id).addEventListener('input', () => {
    updateLabel(id);
    queue({ hue: $('hue').value, sat: $('sat').value });
  });
});

['power', 'bg_power'].forEach(id => {
  $(id).addEventListener('change', () => {
    queue({ [id]: $(id).checked ? 'on' : 'off' });
  });
});

function refresh() {
  if (Date.now() - lastAction < 2500 || inflight) return;  // не мешаем, пока двигаете ползунок
  fetch('/api/state')
    .then(r => r.json())
    .then(j => {
      setStatus(j.ok);
      if (!j.ok) return;
      $('power').checked = (j.power === 'on');
      $('bg_power').checked = (j.bg_power === 'on');
      ['bright', 'ct', 'bg_bright', 'hue', 'sat'].forEach(id => {
        if (j[id] !== undefined) $(id).value = j[id];
        updateLabel(id);
      });
    })
    .catch(() => setStatus(false));
}

['bright', 'ct', 'bg_bright', 'hue', 'sat'].forEach(updateLabel);
refresh();
setInterval(refresh, 3000);
</script>
</body>
</html>
)HTML";
