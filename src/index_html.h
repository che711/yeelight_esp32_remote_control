#pragma once

const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lamp</title>
<style>
  /* The page light comes from the lamp itself: --glow is the color the backlight is showing now */
  @property --glow { syntax:'<color>'; inherits:true; initial-value:#6b7cff; }
  :root { color-scheme: dark; --bg:#0d0f24; --text:#f4f5ff; --dim:#a3a9d6; --line:rgba(255,255,255,.13);
          --field:rgba(255,255,255,.07); --accent:#ffb02e;
          --glow-grad:linear-gradient(90deg,#2a2f55,#4a5170,#2a2f55); --hue-solid:#f00;
          transition:--glow .9s ease; }
  @media (prefers-reduced-motion: reduce) { :root { transition:none; } }
  * { box-sizing: border-box; }
  html { background:var(--bg); }
  body { margin:0; padding:18px 16px 44px; min-height:100vh; color:var(--text);
         font:16px/1.45 system-ui,-apple-system,"Segoe UI",sans-serif; background:var(--bg);
         background:radial-gradient(80% 52% at 50% -8%, color-mix(in srgb, var(--glow) 46%, transparent), transparent 72%) no-repeat fixed, var(--bg); }
  main { max-width:480px; margin:0 auto; }
  h1 { font-size:28px; font-weight:800; letter-spacing:-.02em; margin:2px 0 8px; }
  #fw { font-size:13px; font-weight:400; letter-spacing:0; color:var(--dim); white-space:nowrap; }
  #strip { height:6px; border-radius:3px; background:var(--glow-grad); box-shadow:0 0 26px 2px var(--glow); margin:0 0 10px; }
  #status { font-size:13px; color:var(--dim); margin-bottom:16px; }
  #status.bad { color:#ff7a8a; }

  details.card { background:linear-gradient(180deg,rgba(255,255,255,.08),rgba(255,255,255,.03));
                 border:1px solid var(--line); border-radius:18px; padding:16px 18px; margin-bottom:16px;
                 box-shadow:0 18px 40px -28px var(--tone, var(--glow)); }
  details.card > summary { display:flex; align-items:center; gap:10px; font-size:17px; font-weight:700;
                           cursor:pointer; list-style:none; user-select:none; }
  details.card > summary::-webkit-details-marker { display:none; }
  details.card > summary::before { content:''; width:10px; height:10px; border-radius:50%;
                                   background:var(--tone, var(--glow)); box-shadow:0 0 12px var(--tone, var(--glow)); }
  details.card > summary::after { content:'▾'; margin-left:auto; color:var(--dim); }
  details.card:not([open]) > summary::after { content:'▸'; }
  details.card[open] > summary { margin-bottom:12px; }

  .row { display:flex; justify-content:space-between; align-items:center; margin:14px 0 4px; }
  .row b { color:var(--accent); font-weight:700; }

  input[type=range] { -webkit-appearance:none; appearance:none; width:100%; height:16px; border-radius:8px; margin:8px 0;
                      background:var(--track, linear-gradient(90deg,#2a2f5c,#8b7bff)); outline:none; }
  input[type=range]::-webkit-slider-thumb { -webkit-appearance:none; width:26px; height:26px; border-radius:50%;
        background:#fff; border:3px solid var(--glow); box-shadow:0 0 14px var(--glow), 0 2px 6px rgba(0,0,0,.5); }
  input[type=range]::-moz-range-thumb { width:22px; height:22px; border-radius:50%;
        background:#fff; border:3px solid var(--glow); box-shadow:0 0 14px var(--glow), 0 2px 6px rgba(0,0,0,.5); }
  input[type=range]:focus-visible { outline:2px solid #fff; outline-offset:3px; }
  #bright    { --track:linear-gradient(90deg,#2b2a3d,#fff1c9); }
  #ct        { --track:linear-gradient(90deg,#ff9a3c,#fff2e0,#9ecbff); }
  #sat       { --track:linear-gradient(90deg,#fff,var(--hue-solid)); }
  #hue       { --track:linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00); }
  #flow_s    { --track:linear-gradient(90deg,#3de0c8,#ffd166); }
  #swing     { --track:linear-gradient(90deg,#2a2f5c,#ff6bd6); }
  #sleep_m   { --track:linear-gradient(90deg,#ffb347,#3a2a6b); }

  .pwr { width:100%; margin:4px 0 10px; padding:14px; border:1px solid var(--line); border-radius:14px;
         background:var(--field); color:var(--dim); font:inherit; font-weight:700; cursor:pointer;
         transition:box-shadow .2s, background .2s; }
  .pwr.on { background:linear-gradient(135deg,#ffd35c,#ff8a2a); border-color:transparent; color:#2a1400; box-shadow:0 0 26px -4px #ff9d2e; }
  #bg_power.on { background:linear-gradient(135deg, color-mix(in srgb, var(--glow) 70%, #fff), var(--glow));
                 color:#0b0c1e; box-shadow:0 0 28px -4px var(--glow); }
  .pwr.dim { background:rgba(255,176,46,.14); border-color:var(--accent); color:var(--accent); }
  .pwr:disabled { opacity:.45; cursor:default; }

  .seg { display:flex; gap:8px; flex-wrap:wrap; margin:6px 0 8px; }
  .seg button { position:relative; isolation:isolate; overflow:hidden; flex:1 1 30%; padding:11px 6px 14px;
                border:1px solid var(--line); border-radius:12px; background:var(--field); color:var(--text);
                font:inherit; font-size:14px; cursor:pointer; transition:box-shadow .2s, border-color .2s; }
  .seg button:active { background:rgba(255,255,255,.2); }
  /* The colored strip at the bottom of a button plays like the effect itself; for the active effect it fills the whole button */
  .seg button[data-v]::after { content:''; position:absolute; z-index:-1; left:0; right:0; bottom:0; height:4px;
        background-image:var(--sw); background-size:250% 100%; background-repeat:no-repeat;
        animation:drift 8s ease-in-out infinite alternate; }
  .seg button[data-v="off"]::after { content:none; }
  #white::after { content:''; position:absolute; z-index:-1; left:0; right:0; bottom:0; height:4px;
        background:linear-gradient(90deg,#fff,#d8ecff,#fff); }
  .seg button[data-v="rainbow"]::after { animation-duration:7s; }
  .seg button[data-v="ocean"]::after { animation-duration:10s; }
  .seg button[data-v="fire"]::after { animation:drift 1.6s ease-in-out infinite alternate, flicker-fire .9s linear infinite; }
  .seg button[data-v="candle"]::after { animation:drift 6s ease-in-out infinite alternate, flicker-candle 3.6s ease-in-out infinite; }
  .seg button[data-v="lava"]::after { animation:drift 12s ease-in-out infinite alternate, lava 6s ease-in-out infinite; }
  .seg button[data-v="sunset"]::after { background-size:300% 100%; animation:drift 9s ease-in-out infinite alternate; }
  .seg button[data-v="sunrise"]::after { background-size:300% 100%; animation:drift-rev 9s ease-in-out infinite alternate; }
  .seg button[data-v="storm"]::after { background-size:100% 100%; animation:storm 6s linear infinite; }
  .seg button[data-v="wave"]::after { background-size:100% 100%; animation:pulse 2.4s ease-in-out infinite; }
  .seg button[data-v="heart"]::after { background-size:100% 100%; animation:beat 1.4s ease-out infinite; }
  .seg button[data-v="garland"]::after { background-image:none; background-color:#f00; animation:garland 3.2s steps(1) infinite; }
  .seg button[data-v="police"]::after { background-image:none; background-color:#f00; animation:police 1.6s steps(1) infinite; }
  .seg button.on { background:rgba(8,9,28,.72); color:#fff; font-weight:700; border-color:rgba(255,255,255,.8);
                   text-shadow:0 1px 3px rgba(0,0,0,.7); box-shadow:0 0 20px -2px var(--glow); }
  .seg button.on[data-v]::after { top:0; height:auto; opacity:.62; }
  .seg button[data-v="off"].on { background:#2f3580; box-shadow:none; }
  @keyframes drift { from { background-position:0% 0; } to { background-position:100% 0; } }
  @keyframes drift-rev { from { background-position:100% 0; } to { background-position:0% 0; } }
  @keyframes flicker-fire { 0%,100% { filter:brightness(1); } 12% { filter:brightness(1.45); } 25% { filter:brightness(.7); }
        38% { filter:brightness(1.3); } 50% { filter:brightness(.85); } 63% { filter:brightness(1.55); }
        78% { filter:brightness(.75); } 90% { filter:brightness(1.25); } }
  @keyframes flicker-candle { 0%,100% { filter:brightness(.95); } 20% { filter:brightness(1.12); } 35% { filter:brightness(.85); }
        55% { filter:brightness(1.18); } 75% { filter:brightness(.9); } }
  @keyframes lava { 0%,100% { filter:brightness(.75); } 50% { filter:brightness(1.3); } }
  @keyframes storm { 0%,100% { filter:brightness(.55); } 3% { filter:brightness(2.4); } 5% { filter:brightness(.55); }
        7% { filter:brightness(2); } 9% { filter:brightness(.55); } 57% { filter:brightness(2.6); } 58.5% { filter:brightness(.6); } }
  @keyframes beat { 0%,100% { opacity:.3; } 8% { opacity:1; } 16% { opacity:.4; } 24% { opacity:.9; } 40% { opacity:.3; } }
  @keyframes pulse { 0%,100% { opacity:.25; } 50% { opacity:1; } }
  @keyframes garland { 0%,100% { background-color:#f00; } 25% { background-color:#0c0; } 50% { background-color:#fc0; } 75% { background-color:#00f; } }
  @keyframes police { 0%,100% { background-color:#f00; } 12.5% { background-color:#400; } 25% { background-color:#f00; }
        37.5% { background-color:#400; } 50% { background-color:#00f; } 62.5% { background-color:#004; }
        75% { background-color:#00f; } 87.5% { background-color:#004; } }
  @media (prefers-reduced-motion: reduce) { .seg button::after { animation:none !important; } }

  #swatch { width:100%; height:12px; border-radius:6px; margin-top:12px; box-shadow:0 0 18px -4px var(--glow); }

  .kv { display:flex; justify-content:space-between; gap:12px; margin:9px 0 2px; font-size:14px; }
  .kv span:first-child { color:var(--dim); }
  .kv span:last-child { text-align:right; word-break:break-all; }
  .bar { height:8px; border-radius:4px; background:rgba(255,255,255,.1); overflow:hidden; margin:3px 0 7px; }
  .bar i { display:block; height:100%; width:0; background:linear-gradient(90deg,#3de0c8,#ffd166); transition:width .4s; }
  .bar i.hot { background:linear-gradient(90deg,#ff9a3c,#ff4d6d); }
  #reboot { margin-top:12px; padding:9px 20px; border:1px solid #8a3340; border-radius:10px;
            background:rgba(255,77,109,.14); color:#ffb4c0; font:inherit; cursor:pointer; }
  #reboot:active { background:#ff4d6d; color:#000; }
  #sys_note { font-size:12px; color:var(--dim); margin-top:8px; }

  .ex { margin:12px 0; }
  .ex .t { font-size:13px; color:var(--dim); margin-bottom:5px; }
  .code { display:flex; gap:8px; align-items:stretch; }
  .code code { flex:1; min-width:0; padding:9px 11px; border-radius:10px; background:rgba(0,0,0,.38);
               border:1px solid var(--line); color:#d7f7ff; white-space:pre; overflow-x:auto;
               font:12.5px/1.5 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace; }
  .code button { padding:0 12px; border:1px solid var(--line); border-radius:10px; background:var(--field);
                 color:var(--text); font:inherit; font-size:13px; cursor:pointer; }
  .code button:active { background:linear-gradient(135deg,#ffd35c,#ff8a2a); color:#2a1400; }
  .params { margin-top:16px; font-size:13px; color:var(--dim); }
  .params div { margin:5px 0; }
  .params b { color:var(--text); font:600 12.5px ui-monospace,SFMono-Regular,Menlo,Consolas,monospace; }
  #sleep_note, #scene_note { font-size:13px; color:var(--dim); margin-top:6px; min-height:1em; }
  #sleep_note { color:var(--accent); }
  .scene { display:flex; gap:8px; margin:8px 0; }
  .scene .go { flex:1; padding:11px 14px; text-align:left; border:1px solid var(--line); border-radius:12px;
               background:var(--field); color:var(--text); font:inherit; font-weight:600; cursor:pointer; }
  .scene .go:active { background:linear-gradient(135deg,#ffd35c,#ff8a2a); color:#2a1400; }
  .scene .del { width:46px; border:1px solid var(--line); border-radius:12px; background:var(--field);
                color:var(--dim); font:inherit; cursor:pointer; }
  .scene .del:active { background:#ff4d6d; color:#000; }
  #scene_row { display:flex; gap:8px; margin-top:14px; }
  #scene_name { flex:1; min-width:0; padding:10px 12px; border:1px solid var(--line); border-radius:12px;
                background:var(--field); color:var(--text); font:inherit; }
  #scene_save { padding:10px 16px; border:1px solid var(--line); border-radius:12px; background:var(--field);
                color:var(--text); font:inherit; font-weight:600; cursor:pointer; }
  #scene_save:active { background:linear-gradient(135deg,#ffd35c,#ff8a2a); color:#2a1400; }
</style>
</head>
<body>
<main>
  <h1>Yeelight Screen Light Bar Pro (YLTD003) <span id="fw"></span></h1>
  <div id="strip"></div>
  <div id="status">reading the lamp state…</div>

  <details class="card" id="sec_main" style="--tone:#ffd166" open>
    <summary>Main light</summary>
    <button type="button" id="power" class="pwr" data-on="0" disabled>Main light…</button>
    <div class="row"><span>Brightness</span><b id="bright_v"></b></div>
    <input type="range" id="bright" min="1" max="100" autocomplete="off">
    <div class="row"><span>Color temperature</span><b id="ct_v"></b></div>
    <input type="range" id="ct" min="2700" max="6500" step="100" autocomplete="off">
  </details>

  <details class="card" id="sec_bg" style="--tone:var(--glow)" open>
    <summary>Backlight</summary>
    <button type="button" id="bg_power" class="pwr" data-on="0" disabled>Backlight…</button>
    <div class="row"><span>Effect</span></div>
    <div id="bg_effect">
    <!-- The effect and color-cycling buttons are built from the EFFECTS table in the script -->
    <div class="seg" id="seg_fx">
      <button type="button" data-v="off" class="on">None</button>
      <button type="button" id="white">White</button>
    </div>
    <div class="row"><span>Color cycling</span></div>
    <div class="seg" id="seg_pal"></div>
    </div>
    <div class="seg"><button type="button" id="bg_lock">Lock color</button></div>
    <div class="row"><span id="flow_name">Step time</span><b id="flow_s_v"></b></div>
    <input type="range" id="flow_s" min="1" max="20" value="3" autocomplete="off">
    <div class="row"><span>Brightness swing (color cycling)</span><b id="swing_v"></b></div>
    <input type="range" id="swing" min="0" max="90" value="0" autocomplete="off">
    <div class="row"><span>Color</span><b id="hue_v"></b></div>
    <input type="range" id="hue" min="0" max="359" autocomplete="off">
    <div class="row"><span>Saturation</span><b id="sat_v"></b></div>
    <input type="range" id="sat" min="0" max="100" autocomplete="off">
    <div id="swatch"></div>
    <div class="row"><span>Sleep timer</span><b id="sleep_m_v"></b></div>
    <input type="range" id="sleep_m" min="1" max="120" value="30" autocomplete="off">
    <div class="seg">
      <button type="button" id="sleep_go">Start timer</button>
      <button type="button" id="sleep_stop">Cancel</button>
    </div>
    <div id="sleep_note"></div>
  </details>

  <details class="card" id="sec_scenes" style="--tone:#9b8cff" open>
    <summary>Backlight scenes</summary>
    <div id="scene_list"></div>
    <div id="scene_row">
      <input type="text" id="scene_name" maxlength="20" placeholder="Scene name" autocomplete="off">
      <button type="button" id="scene_save">Save current</button>
    </div>
    <div id="scene_note"></div>
  </details>

  <details class="card" id="sec_api" style="--tone:#ff7ab8">
    <summary>Terminal API (curl)</summary>
    <div class="kv"><span>Board address</span><span id="api_base">—</span></div>
    <div class="ex">
      <div class="t">Current backlight settings as one command</div>
      <div class="code"><code id="api_now"></code><button type="button" id="api_now_copy">Copy</button></div>
    </div>
    <div id="api_list"></div>
    <div class="params" id="api_params"></div>
  </details>

  <details class="card" id="sec_sys" style="--tone:#3de0c8" open>
    <summary>System</summary>
    <div class="kv"><span>Firmware version</span><span id="sys_version">—</span></div>
    <div class="kv"><span>Board</span><span id="sys_chip">—</span></div>
    <div class="kv"><span>Uptime</span><span id="sys_uptime">—</span></div>
    <div class="kv"><span>Memory (heap)</span><span id="sys_heap">—</span></div>
    <div class="bar"><i id="sys_heap_bar"></i></div>
    <div class="kv"><span>Minimum free memory</span><span id="sys_heap_min">—</span></div>
    <div class="kv"><span>Firmware size</span><span id="sys_sketch">—</span></div>
    <div class="bar"><i id="sys_sketch_bar"></i></div>
    <div class="kv"><span>Server load</span><span id="sys_busy">—</span></div>
    <div class="bar"><i id="sys_busy_bar"></i></div>
    <div class="kv"><span>Chip temperature</span><span id="sys_temp">—</span></div>
    <div class="kv"><span>Wi-Fi</span><span id="sys_wifi">—</span></div>
    <div class="kv"><span>IP address</span><span id="sys_ip">—</span></div>
    <div class="kv"><span>MAC address</span><span id="sys_mac">—</span></div>
    <button id="reboot" type="button">Reboot the board</button>
    <div id="sys_note"></div>
  </details>
</main>

<script>
const $ = id => document.getElementById(id);
// The backlight brightness is always 100%: there is no brightness slider for the backlight
const BG_BRIGHT = 100;
const suffix = { bright:'%', ct:' K', hue:'°', sat:'%', flow_s:' s', swing:'%', sleep_m:' min' };

// All backlight effects in one table; the row order is the button order.
// A new effect: a row here + a row in FLOW_DEFS in main.cpp (the animation of the button strip can be added in CSS).
//   board - the name sent to the board (for the rainbow it is "on"); label - button text;
//   group - 'fx' (the "Effect" block) or 'pal' (the "Color cycling" block);
//   kind  - 'color': the color comes from the sliders; 'timed': runs once over the given time (minutes);
//           'palette': "Brightness swing" works; no kind - a plain effect;
//   sw - a sample strip with the effect's colors; glow - the page glow color;
//   btn - the strip on the button, if it must differ from sw.
const EFFECTS = {
  wave:    { board: 'wave',    label: 'Wave',      group: 'fx',  kind: 'color',
             btn: 'linear-gradient(90deg,#1b1b2b,#ff6bd6,#1b1b2b)' },
  fire:    { board: 'fire',    label: 'Campfire',  group: 'fx',
             sw: 'linear-gradient(90deg,#c21800,#ff6a00,#ffb400,#ff3c00,#ff8c00,#ff2800)', glow: '#ff6a00' },
  candle:  { board: 'candle',  label: 'Candle',    group: 'fx',
             sw: 'linear-gradient(90deg,#ff8a1f,#ffb04a,#ff8a1f,#ffa23a,#ff8a1f)', glow: '#ffa23a' },
  sunset:  { board: 'sunset',  label: 'Sunset',    group: 'fx',  kind: 'timed',
             sw: 'linear-gradient(90deg,#ffe9c8,#ffb347,#ff7a18,#e8431a,#8b1a1a,#300606)', glow: '#ff7a18' },
  sunrise: { board: 'sunrise', label: 'Sunrise',   group: 'fx',  kind: 'timed',
             sw: 'linear-gradient(90deg,#300606,#8b1a1a,#e8431a,#ff7a18,#ffb347,#ffe9c8)', glow: '#ffb347' },
  storm:   { board: 'storm',   label: 'Storm',     group: 'fx',
             sw: 'linear-gradient(90deg,#0b1238,#fff,#0b1238,#0a0f2e,#fff,#101c4a)', glow: '#7aa2ff' },
  ocean:   { board: 'ocean',   label: 'Ocean',     group: 'fx',
             sw: 'linear-gradient(90deg,#00a8a8,#0066cc,#2a00b8,#00c878,#00b4d8,#5a1ab8)', glow: '#00c8c0' },
  lava:    { board: 'lava',    label: 'Lava',      group: 'fx',
             sw: 'linear-gradient(90deg,#600000,#c81e00,#ff7a00,#c81e00,#600000)', glow: '#ff3a00' },
  heart:   { board: 'heart',   label: 'Heartbeat', group: 'fx',  kind: 'color',
             btn: 'linear-gradient(90deg,#3a0014,#ff2d55,#3a0014)' },
  garland: { board: 'garland', label: 'Garland',   group: 'fx',
             sw: 'linear-gradient(90deg,#f00 0 25%,#0c0 25% 50%,#fc0 50% 75%,#00f 75%)', glow: '#38d66b' },
  police:  { board: 'police',  label: 'Police',    group: 'fx',
             sw: 'linear-gradient(90deg,#f00 0 50%,#00f 50%)', glow: '#ff3040' },
  rainbow: { board: 'on',      label: 'Rainbow',   group: 'pal', kind: 'palette',
             sw: 'linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00)', glow: '#b46cff' },
  warm:    { board: 'warm',    label: 'Warm',      group: 'pal', kind: 'palette',
             sw: 'linear-gradient(90deg,#ff3b00,#ff6a00,#ffa000,#ffc833,#ff2d55,#ff8a3d)', glow: '#ff7a2a' },
  cold:    { board: 'cold',    label: 'Cold',      group: 'pal', kind: 'palette',
             sw: 'linear-gradient(90deg,#0066ff,#00b4ff,#00e0d0,#6a5cff,#00a0a0,#3aa0ff)', glow: '#2aa8ff' },
  neon:    { board: 'neon',    label: 'Neon',      group: 'pal', kind: 'palette',
             sw: 'linear-gradient(90deg,#f0f,#0ff,#39ff14,#ff1493,#1f51ff,#ff0)', glow: '#ff2fe0' },
  pastel:  { board: 'pastel',  label: 'Pastel',    group: 'pal', kind: 'palette',
             sw: 'linear-gradient(90deg,#ffb3ba,#ffdfba,#ffffba,#baffc9,#bae1ff,#e0bbff)', glow: '#ffb3d9' }
};

// Everything else is derived from the table
const SWATCHES = {};          // sample strip
const GLOW = {};              // page glow color (effects with the color from the sliders have none)
const EFFECT_TO_BOARD = {};   // name on the page -> name for the board
const EFFECT_FROM_BOARD = {}; // name for the board -> name on the page
const COLOR_EFFECTS = [];     // the color is chosen with the sliders
const PALETTE_EFFECTS = [];   // color cycling: "Brightness swing" works for them
const TIMED_EFFECTS = [];     // run once over the given time
for (const [key, e] of Object.entries(EFFECTS)) {
  EFFECT_TO_BOARD[key] = e.board;
  EFFECT_FROM_BOARD[e.board] = key;
  if (e.sw) SWATCHES[key] = e.sw;
  if (e.glow) GLOW[key] = e.glow;
  if (e.kind === 'color') COLOR_EFFECTS.push(key);
  if (e.kind === 'palette') PALETTE_EFFECTS.push(key);
  if (e.kind === 'timed') TIMED_EFFECTS.push(key);
  // The effect button with a colored strip: you can see which colors it gives
  const b = document.createElement('button');
  b.type = 'button';
  b.dataset.v = key;
  b.textContent = e.label;
  const g = e.btn || e.sw;
  if (g) b.style.setProperty('--sw', g);
  $(e.group === 'pal' ? 'seg_pal' : 'seg_fx').append(b);
}

let flowSec = 3;    // time of one step, seconds (for plain effects)
let flowMin = 10;   // total duration, minutes (for sunset and sunrise)
let lastAction = 0;
let inflight = false;
let queued = {};
let urgent = false;         // a command from a button: send at once, without waiting for the pause between requests
let stateLoaded = false;    // the lamp state has been read at least once

// ---------- Power buttons ----------
// The state is kept in data-on, and the button text and color change with it

function isOn(id) { return $(id).dataset.on === '1'; }

function setOn(id, on) {
  const b = $(id);
  b.dataset.on = on ? '1' : '0';
  b.classList.toggle('on', on);
  b.textContent = on ? 'Backlight: on' : 'Backlight: off';
  updateGlow();
}

// The main light has three states: on - lit, dim - lit at the minimum brightness (the slider is at 1%),
// off - turned off. The button toggles between lit (on or dim) and off.
let mainMode = 'off';
let mainSaved = 70;  // brightness before turning off
const MAIN_LABELS = { on: 'Main light: on', dim: 'Main light: dimmed', off: 'Main light: off' };

function setMain(mode) {
  mainMode = mode;
  const b = $('power');
  b.dataset.on = mode === 'on' ? '1' : '0';
  b.classList.toggle('on', mode === 'on');
  b.classList.toggle('dim', mode === 'dim');
  b.textContent = MAIN_LABELS[mode];
}

// ---------- Collapsible blocks ----------

document.querySelectorAll('details.card').forEach(d => {
  try {
    const saved = localStorage.getItem('open_' + d.id);
    if (saved === '0') d.open = false;
    else if (saved === '1') d.open = true;
  } catch (e) {}
  d.addEventListener('toggle', () => {
    try { localStorage.setItem('open_' + d.id, d.open ? '1' : '0'); } catch (e) {}
    if (d.id === 'sec_sys' && d.open) loadSystem();
    if (d.id === 'sec_api' && d.open) renderApiNow();
  });
});

// ---------- Lamp ----------

function setStatus(ok) {
  const s = $('status');
  s.textContent = ok ? 'lamp connected' : 'lamp not responding';
  s.className = ok ? '' : 'bad';
}

function effect() {  // off or the name of an effect
  const b = document.querySelector('#bg_effect button.on');
  return b ? b.dataset.v : 'off';
}
function setEffect(v) {
  document.querySelectorAll('#bg_effect button[data-v]').forEach(b => b.classList.toggle('on', b.dataset.v === v));
  applyFlowSlider();
}
function flowOn() { return effect() !== 'off'; }
function usesColor() { return COLOR_EFFECTS.includes(effect()); }
function isTimed() { return TIMED_EFFECTS.includes(effect()); }
function isPalette() { return PALETTE_EFFECTS.includes(effect()); }

// The time slider: for sunset and sunrise it is the duration in minutes, for the rest the time of one step in seconds
function applyFlowSlider() {
  const t = isTimed();
  $('flow_s').max = t ? 30 : 20;
  $('flow_s').value = t ? flowMin : flowSec;
  $('flow_name').textContent = t ? 'Duration' : 'Step time';
  suffix.flow_s = t ? ' min' : ' s';
  updateLabel('flow_s');
}

// Parameters for starting an effect: the lamp cycles the colors itself.
// All effects need the speed and brightness; wave and heartbeat also need the color.
function flowParams() {
  const p = { bg_flow: EFFECT_TO_BOARD[effect()], flow_ms: $('flow_s').value * (isTimed() ? 60000 : 1000),
              bg_bright: BG_BRIGHT };
  if (usesColor()) { p.hue = $('hue').value; p.sat = $('sat').value; }
  if (isPalette()) p.swing = $('swing').value;
  return p;
}

function updateSwatch() {
  const col = hsvCss(+$('hue').value, +$('sat').value);
  const e = effect();
  $('swatch').style.background = SWATCHES[e]
    || (COLOR_EFFECTS.includes(e) ? 'linear-gradient(90deg,#000,' + col + ',#000)' : col);
  document.documentElement.style.setProperty('--hue-solid', hsvCss(+$('hue').value, 100));
  updateGlow();
  renderApiNow();
}

// The whole page glows with the backlight color: background, the strip under the title, the thumbs. Backlight off: a muted gray-blue
function updateGlow() {
  const root = document.documentElement.style;
  const e = effect();
  const col = hsvCss(+$('hue').value, +$('sat').value);
  let glow, grad;
  if (!isOn('bg_power')) {
    glow = '#4a5170';
    grad = 'linear-gradient(90deg,#2a2f55,#4a5170,#2a2f55)';
  } else if (e === 'off') {
    glow = col;
    grad = 'linear-gradient(90deg,' + col + ',' + col + ')';
  } else if (COLOR_EFFECTS.includes(e)) {
    glow = col;
    grad = 'linear-gradient(90deg,#000,' + col + ',#000)';
  } else {
    glow = GLOW[e];
    grad = SWATCHES[e];
  }
  root.setProperty('--glow', glow);
  root.setProperty('--glow-grad', grad);
}

function updateLabel(id) {
  $(id + '_v').textContent = $(id).value + suffix[id];
  if (id === 'hue' || id === 'sat') updateSwatch();
  renderApiNow();
}

// The reverse conversion: a 0xRRGGBB number from the lamp to hue and saturation for the sliders
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

// HSV (brightness 100%) to a color for the sample strip; at saturation 0 the result is white
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

// The lamp accepts about 60 commands per minute; above that it goes silent for a minute or more.
// So requests from the sliders go out no more than once per 1.2 s, and while a slider moves only the last value is kept.
// Button and switch presses are sent at once (urgent): they are rare and there is no reason to wait.
const MIN_GAP_MS = 1200;
let lastSent = 0;
let timer = null;
let syncAfterSend = false;  // after a command read the lamp state: 'lock' - the color too, 'state' - only the switches

function queue(params, now) {
  // Any other backlight command cancels the sleep timer on the lamp, and so the countdown on the page
  if (!('bg_sleep' in params) && ['hue', 'bg_flow', 'bg_bright', 'bg_power'].some(k => k in params)) sleepEnd = 0;
  Object.assign(queued, params);
  lastAction = Date.now();
  if (now) urgent = true;
  schedule();
}

function schedule() {
  if (inflight) return;  // after the reply flush() calls schedule() again
  if (timer) {
    if (!urgent) return;
    clearTimeout(timer);
    timer = null;
  }
  const wait = urgent ? 0 : Math.max(0, MIN_GAP_MS - (Date.now() - lastSent));
  urgent = false;
  timer = setTimeout(() => { timer = null; flush(); }, wait);
}

// fetch with a time limit: without it a hung request blocks the command queue forever
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
      else if (syncAfterSend) {
        const mode = syncAfterSend;
        syncAfterSend = false;
        setTimeout(() => refresh(true, mode === 'lock'), MIN_GAP_MS);
      }
    });
}

['bright', 'ct'].forEach(id => {
  $(id).addEventListener('input', () => {
    updateLabel(id);
    if (id === 'bright') {
      const v = +$('bright').value;
      if (v > 1) mainSaved = v;
      if (mainMode !== 'off') setMain(v <= 1 ? 'dim' : 'on');
    }
    queue({ [id]: $(id).value });
  });
});

// Effect buttons: a press starts the effect at once or stops it
$('bg_effect').addEventListener('click', e => {
  const b = e.target.closest('button[data-v]');
  if (!b || b.dataset.v === effect()) return;
  setEffect(b.dataset.v);
  updateSwatch();
  if (flowOn()) setOn('bg_power', true);  // an effect turns the backlight on by itself
  queue(flowOn() ? flowParams() : { bg_flow: 'off' }, true);
});

$('swing').addEventListener('input', () => {
  updateLabel('swing');
  if (isPalette()) queue(flowParams());
});

// Stop the effect on the color that is lit now. The button stays in place and always works
$('bg_lock').addEventListener('click', () => {
  if (flowOn()) {
    setEffect('off');
    updateSwatch();
    syncAfterSend = 'lock';
    queue({ bg_flow: 'off' }, true);
  } else {
    refresh(true, true);  // no effect: just read the color from the lamp into the sliders
  }
});

$('flow_s').addEventListener('input', () => {
  if (isTimed()) flowMin = +$('flow_s').value; else flowSec = +$('flow_s').value;
  updateLabel('flow_s');
  if (flowOn()) queue(flowParams());
});

// Picking a color by hand turns off color cycling and the campfire; the wave just changes its color.
// The brightness is sent together with the color: the server leaves the effect with one command.
function leaveRainbow() {
  if (effect() === 'off' || usesColor()) return {};
  setEffect('off');
  return { bg_flow: 'off', bg_bright: BG_BRIGHT };
}

['hue', 'sat'].forEach(id => {
  $(id).addEventListener('input', () => {
    const stop = leaveRainbow();
    // At saturation 0 the color is always white, so the hue slider would change nothing.
    // If the hue is moved after "White", we put the saturation back to the maximum.
    if (id === 'hue' && +$('sat').value === 0) {
      $('sat').value = 100;
      updateLabel('sat');
    }
    updateLabel(id);
    queue(usesColor() ? flowParams() : Object.assign({ hue: $('hue').value, sat: $('sat').value }, stop));
  });
});

$('white').addEventListener('click', () => {
  const stop = leaveRainbow();
  $('sat').value = 0;
  updateLabel('sat');
  queue(usesColor() ? flowParams() : Object.assign({ hue: $('hue').value, sat: 0 }, stop), true);
});

// The lamp has no command to turn off only the main light: it turns both lights off at once.
// So to turn off the main light we turn everything off and, in the same request, set the backlight again.
// The backlight blinks for a moment; a running effect starts over, a sleep timer is restarted with the time left.
function backlightParams() {
  if (!isOn('bg_power')) return {};
  const color = { hue: $('hue').value, sat: $('sat').value, bg_bright: BG_BRIGHT };
  const left = sleepEnd ? Math.round((sleepEnd - Date.now()) / 1000) : 0;
  if (left > 0) return Object.assign({ bg_sleep: Math.max(1, Math.ceil(left / 60)) }, color);
  return flowOn() ? flowParams() : Object.assign({ bg_power: 'on', bg_check: 0 }, color);  // bg_check=0: skip the check, it saves a command
}

$('power').addEventListener('click', () => {
  syncAfterSend = 'state';  // in a second we check the buttons against the lamp
  if (mainMode !== 'off') {
    // Turn the main light off; the backlight is brought back by the same request
    const cur = +$('bright').value;
    if (cur > 1) mainSaved = cur;
    setMain('off');
    const params = Object.assign({ power: 'off' }, backlightParams());
    queue(params, true);
    if (params.bg_sleep) sleepEnd = Date.now() + params.bg_sleep * 60000;  // the timer was restarted
  } else {
    // Turn the lamp on and return the previous brightness
    $('bright').value = mainSaved;
    updateLabel('bright');
    setMain('on');
    const params = { main: 'restore', power: 'on' };
    if (!isOn('bg_power')) params.bg_power = 'off';  // the lamp turns both lights on, but the backlight was off
    queue(params, true);
  }
});

$('bg_power').addEventListener('click', () => {
  const on = !isOn('bg_power');
  setOn('bg_power', on);
  if (!on) {
    setEffect('off');
    updateSwatch();
    queue({ bg_power: 'off' }, true);
  } else if (flowOn()) {
    // If an effect is chosen, the backlight turns on with it at once
    queue(flowParams(), true);
  } else {
    // When turning the backlight on, set the color at once: it lights up in color instead of the lamp's "last" mode
    queue({ bg_power: 'on', hue: $('hue').value, sat: $('sat').value, bg_bright: BG_BRIGHT }, true);
  }
});

// ---------- Lamp state ----------

let refreshing = false;

// force - do not wait for the pause after actions; applyColor - also set the color sliders to the lamp's color
function refresh(force, applyColor) {
  if (inflight || timer || refreshing) return;
  if (!force && Date.now() - lastAction < 5000) return;  // do not interfere while a slider is being moved
  refreshing = true;
  const actionAtStart = lastAction;
  fetchJson('/api/state', 10000)
    .then(j => {
      setStatus(j.ok);
      if (!j.ok) return;
      if (!force && lastAction !== actionAtStart) return;  // the sliders were touched while the request ran: the reply is stale
      if (j.main_saved > 1) mainSaved = j.main_saved;
      setMain(j.power !== 'on' ? 'off' : (j.bright <= 1 ? 'dim' : 'on'));
      setOn('bg_power', j.bg_power === 'on');
      if (!stateLoaded) {  // the power buttons wait for the first lamp reply so they do not show an invented state
        stateLoaded = true;
        $('power').disabled = false;
        $('bg_power').disabled = false;
      }
      // The lamp tells whether an effect is running; which one is remembered by the board. If the board rebooted, we assume color cycling
      let flowing = false;
      if (j.bg_flow === '0') {
        setEffect('off');
        sleepEnd = 0;
        renderSleep();
      } else if (j.bg_flow === '1' && j.effect === 'sleep') {
        flowing = true;  // the sleep timer is running: there is no effect, but there is a countdown
        setEffect('off');
        if (j.sleep_left > 0) { sleepEnd = Date.now() + j.sleep_left * 1000; renderSleep(); }
      } else if (j.bg_flow === '1') {
        flowing = true;
        const known = EFFECT_FROM_BOARD[j.effect];
        setEffect(known || 'rainbow');
        if (known && j.swing !== undefined) {
          $('swing').value = j.swing;
          updateLabel('swing');
        }
        if (known && j.flow_ms) {
          if (TIMED_EFFECTS.includes(known)) flowMin = Math.min(30, Math.max(1, Math.round(j.flow_ms / 60000)));
          else flowSec = Math.min(20, Math.max(1, Math.round(j.flow_ms / 1000)));
          applyFlowSlider();
        }
      }
      ['bright', 'ct', 'hue', 'sat'].forEach(id => {
        // During an effect the lamp color keeps changing, so the color sliders are left alone
        const skip = flowing && (id === 'hue' || id === 'sat');
        if (!skip && j[id] !== undefined) $(id).value = j[id];
        updateLabel(id);
      });
      if (flowing && usesColor() && j.effect === effect()) {
        $('hue').value = j.fhue;
        $('sat').value = j.fsat;
        updateLabel('hue');
        updateLabel('sat');
      }
      // After locking, set the color sliders to the color where the lamp stopped
      if (applyColor === true && j.rgb > 0) {
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

// ---------- Terminal API ----------

const API_EXAMPLES = [
  ['Lamp state', b => `curl ${b}/api/state`],
  ['Dim the main light', b => `curl "${b}/api/set?main=dim"`],
  ['Restore the main light brightness', b => `curl "${b}/api/set?main=restore"`],
  ['Main light brightness and temperature', b => `curl "${b}/api/set?bright=60&ct=4000"`],
  ['Turn the backlight on in red', b => `curl "${b}/api/set?bg_power=on&hue=0&sat=100&bg_bright=80"`],
  ['White backlight', b => `curl "${b}/api/set?bg_power=on&hue=0&sat=0&bg_bright=80"`],
  ['Campfire, 2 second step', b => `curl "${b}/api/set?bg_flow=fire&flow_ms=2000&bg_bright=80"`],
  ['Neon with brightness swing', b => `curl "${b}/api/set?bg_flow=neon&flow_ms=3000&swing=40&bg_bright=90"`],
  ['Blue wave', b => `curl "${b}/api/set?bg_flow=wave&flow_ms=3000&hue=220&sat=100&bg_bright=80"`],
  ['Sunset over 15 minutes', b => `curl "${b}/api/set?bg_flow=sunset&flow_ms=900000&bg_bright=70"`],
  ['Sleep timer for 30 minutes', b => `curl "${b}/api/set?bg_sleep=30&hue=30&sat=80&bg_bright=60"`],
  ['Stop the effect', b => `curl "${b}/api/set?bg_flow=off"`],
  ['Turn the backlight off', b => `curl "${b}/api/set?bg_power=off"`],
  ['Turn everything off', b => `curl "${b}/api/set?power=off"`],
  ['List of saved scenes', b => `curl ${b}/api/scenes`],
  ['Start a scene by name', b => `curl -G "${b}/api/scene" --data-urlencode "name=Evening"`],
  ['Board information', b => `curl ${b}/api/system`],
  ['Reboot the board', b => `curl -X POST ${b}/api/reboot`],
];

// The effect names in the help text come from the EFFECTS table
const boardNames = keys => keys.map(k => EFFECTS[k].board).join(', ');
const API_PARAMS = [
  ['power=on|off', 'both lights at once (main light and backlight)'],
  ['main=dim|restore', 'dim the main light to the minimum or return its previous brightness'],
  ['bright=1..100, ct=2700..6500', 'main light brightness and color temperature'],
  ['bg_power=on|off', 'turn the backlight on or off; the color can be set at the same time'],
  ['hue=0..359, sat=0..100, bg_bright=1..100', 'backlight color, saturation and brightness'],
  ['bg_check=0', 'with bg_power=on: skip the check that the backlight really turned on (saves a command)'],
  ['bg_flow=<effect>|off', Object.values(EFFECTS).map(e => e.board === 'on' ? 'on (rainbow)' : e.board).join(', ')],
  ['flow_ms', 'time of one step in milliseconds; for ' + boardNames(TIMED_EFFECTS) + ' the total duration'],
  ['swing=0..90', 'brightness swing of color cycling (' + boardNames(PALETTE_EFFECTS) + ')'],
  ['hue, sat together with bg_flow', 'the color for ' + boardNames(COLOR_EFFECTS)],
  ['bg_sleep=1..240', 'sleep timer: the backlight fades out over this many minutes; the color and brightness come from hue, sat, bg_bright'],
];

// Copying: on a page over http (not https) navigator.clipboard is unavailable, so there is a fallback
function copyText(text, btn) {
  const done = () => {
    const old = btn.textContent;
    btn.textContent = 'Copied';
    setTimeout(() => { btn.textContent = old; }, 1200);
  };
  const fallback = () => {
    const ta = document.createElement('textarea');
    ta.value = text;
    ta.style.position = 'fixed';
    ta.style.opacity = '0';
    document.body.append(ta);
    ta.select();
    try { if (document.execCommand('copy')) done(); } catch (e) {}
    ta.remove();
  };
  if (navigator.clipboard && window.isSecureContext) navigator.clipboard.writeText(text).then(done).catch(fallback);
  else fallback();
}

function renderApi() {
  const base = location.origin;
  $('api_base').textContent = base;
  const list = $('api_list');
  list.textContent = '';
  API_EXAMPLES.forEach(([title, make]) => {
    const cmd = make(base);
    const wrap = document.createElement('div');
    wrap.className = 'ex';
    const t = document.createElement('div');
    t.className = 't';
    t.textContent = title;
    const row = document.createElement('div');
    row.className = 'code';
    const code = document.createElement('code');
    code.textContent = cmd;
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.textContent = 'Copy';
    btn.addEventListener('click', () => copyText(cmd, btn));
    row.append(code, btn);
    wrap.append(t, row);
    list.append(wrap);
  });
  const params = $('api_params');
  params.textContent = '';
  const head = document.createElement('div');
  head.textContent = '/api/set parameters (they can be combined with &):';
  params.append(head);
  API_PARAMS.forEach(([name, text]) => {
    const line = document.createElement('div');
    const b = document.createElement('b');
    b.textContent = name;
    line.append(b, ' - ' + text);
    params.append(line);
  });
}

// A command that repeats the current backlight settings from the page
function renderApiNow() {
  if (!$('sec_api').open) return;
  const q = flowOn()
    ? new URLSearchParams(flowParams())
    : new URLSearchParams({ bg_power: 'on', hue: $('hue').value, sat: $('sat').value, bg_bright: BG_BRIGHT });
  $('api_now').textContent = 'curl "' + location.origin + '/api/set?' + q.toString() + '"';
}

$('api_now_copy').addEventListener('click', () => copyText($('api_now').textContent, $('api_now_copy')));

// ---------- System ----------

const kb = n => (n / 1024).toFixed(0) + ' KB';

function setBar(id, pct) {
  const el = $(id);
  el.style.width = Math.max(0, Math.min(100, pct)) + '%';
  el.classList.toggle('hot', pct >= 85);
}

function fmtUptime(sec) {
  const d = Math.floor(sec / 86400), h = Math.floor(sec % 86400 / 3600), m = Math.floor(sec % 3600 / 60);
  if (d) return d + ' d ' + h + ' h';
  if (h) return h + ' h ' + m + ' min';
  return m + ' min ' + (sec % 60) + ' s';
}

function wifiQuality(rssi) {
  if (rssi >= -55) return 'excellent';
  if (rssi >= -67) return 'good';
  if (rssi >= -75) return 'fair';
  return 'weak';
}

let sysBusy = false;

// The firmware version is shown next to the title and in the System block
function showVersion(j) {
  if (!j.version) return;
  $('fw').textContent = 'v' + j.version;
  $('sys_version').textContent = j.version + (j.build ? ' (built ' + j.build + ')' : '');
}

// The version is read once at start, it does not touch the lamp
function loadVersion() {
  fetchJson('/api/system', 8000).then(showVersion).catch(() => setTimeout(loadVersion, 5000));
}

function loadSystem() {
  if (sysBusy || !$('sec_sys').open) return;  // a collapsed block is not polled
  sysBusy = true;
  fetchJson('/api/system', 8000)
    .then(j => {
      showVersion(j);
      $('sys_chip').textContent = j.chip + ', ' + j.cores + (j.cores === 1 ? ' core, ' : ' cores, ') + j.cpu_mhz + ' MHz';
      $('sys_uptime').textContent = fmtUptime(j.uptime);
      const used = j.heap_total - j.heap_free;
      $('sys_heap').textContent = 'used ' + kb(used) + ' of ' + kb(j.heap_total);
      setBar('sys_heap_bar', used / j.heap_total * 100);
      $('sys_heap_min').textContent = kb(j.heap_min);
      if (j.sketch_free > 0) {
        $('sys_sketch').textContent = kb(j.sketch) + ' of ' + kb(j.sketch_free);
        setBar('sys_sketch_bar', j.sketch / j.sketch_free * 100);
      } else {
        $('sys_sketch').textContent = kb(j.sketch);
        setBar('sys_sketch_bar', 0);
      }
      $('sys_busy').textContent = j.busy + '%';
      setBar('sys_busy_bar', j.busy);
      $('sys_temp').textContent = j.temp === null ? 'n/a' : j.temp.toFixed(1) + ' °C';
      $('sys_wifi').textContent = j.ssid + ', ' + j.rssi + ' dBm (' + wifiQuality(j.rssi) + ')';
      $('sys_ip').textContent = j.ip;
      $('sys_mac').textContent = j.mac;
      $('sys_note').textContent = '';
    })
    .catch(() => { $('sys_note').textContent = 'the board did not respond, trying again'; })
    .finally(() => { sysBusy = false; });
}

$('reboot').addEventListener('click', () => {
  if (!confirm('Reboot the board? The page will be unavailable for about 10 seconds.')) return;
  fetch('/api/reboot', { method: 'POST' }).catch(() => {});
  $('sys_note').textContent = 'the board is rebooting…';
  setTimeout(() => location.reload(), 10000);
});

// ---------- Sleep timer ----------

let sleepEnd = 0;  // when the backlight goes out (by the page clock), 0 - no timer is running

function renderSleep() {
  const left = sleepEnd ? Math.round((sleepEnd - Date.now()) / 1000) : 0;
  if (left <= 0) {
    if (sleepEnd) {  // time is up: in a couple of seconds check the switches against the lamp
      sleepEnd = 0;
      setTimeout(() => refresh(true), 2500);
    }
    $('sleep_note').textContent = '';
    return;
  }
  const m = Math.floor(left / 60), s = left % 60;
  $('sleep_note').textContent = 'turns off in ' + (m ? m + ' min ' : '') + s + ' s';
}
setInterval(renderSleep, 1000);

$('sleep_m').addEventListener('input', () => updateLabel('sleep_m'));

$('sleep_go').addEventListener('click', () => {
  const m = +$('sleep_m').value;
  setEffect('off');
  updateSwatch();
  setOn('bg_power', true);
  queue({ bg_sleep: m, hue: $('hue').value, sat: $('sat').value, bg_bright: BG_BRIGHT }, true);
  sleepEnd = Date.now() + m * 60000;
  renderSleep();
});

// Cancel returns the backlight to the color and brightness from the sliders
$('sleep_stop').addEventListener('click', () => {
  if (!sleepEnd) return;
  queue({ bg_flow: 'off', hue: $('hue').value, sat: $('sat').value, bg_bright: BG_BRIGHT }, true);
  renderSleep();
});

// ---------- Scenes ----------

const SCENE_MAX = 8;
let scenes = [];

function sceneNote(text) { $('scene_note').textContent = text || ''; }

// Data from the board is checked and brought to valid values: the page must not break on a malformed record
function cleanScene(x) {
  if (!x || typeof x.n !== 'string') return null;
  const num = (v, lo, hi, d) => { v = +v; return isFinite(v) ? Math.min(hi, Math.max(lo, v)) : d; };
  const known = x.e === 'off' || Object.prototype.hasOwnProperty.call(EFFECTS, x.e);
  return { n: x.n.slice(0, 20), e: known ? x.e : 'off', h: num(x.h, 0, 359, 0), s: num(x.s, 0, 100, 100),
           b: num(x.b, 1, 100, 100), fs: num(x.fs, 1, 20, 3), fm: num(x.fm, 1, 30, 10), w: num(x.w, 0, 90, 0) };
}

function renderScenes() {
  const box = $('scene_list');
  box.textContent = '';
  scenes.forEach((sc, i) => {
    const row = document.createElement('div');
    row.className = 'scene';
    const go = document.createElement('button');
    go.type = 'button';
    go.className = 'go';
    go.textContent = sc.n;
    go.addEventListener('click', () => applyScene(sc));
    const del = document.createElement('button');
    del.type = 'button';
    del.className = 'del';
    del.textContent = '✕';
    del.title = 'Delete';
    del.addEventListener('click', () => { scenes.splice(i, 1); renderScenes(); saveScenes(); });
    row.append(go, del);
    box.append(row);
  });
  sceneNote(scenes.length ? '' : 'No saved scenes yet');
}

function saveScenes() {
  return fetch('/api/scenes', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(scenes) })
    .then(r => r.json())
    .then(j => { if (!j.ok) throw new Error('save'); })
    .catch(() => sceneNote('could not save the scenes on the board'));
}

function loadScenes() {
  fetchJson('/api/scenes', 8000)
    .then(a => { scenes = (Array.isArray(a) ? a : []).map(cleanScene).filter(Boolean).slice(0, SCENE_MAX); renderScenes(); })
    .catch(() => { sceneNote('could not read the scenes, trying again'); setTimeout(loadScenes, 5000); });
}

// Start a scene: color, brightness, effect and speed are set with one command
function applyScene(sc) {
  $('hue').value = sc.h;
  $('sat').value = sc.s;
  $('swing').value = sc.w;
  flowSec = sc.fs;
  flowMin = sc.fm;
  setEffect(sc.e);  // this also sets the time slider
  ['hue', 'sat', 'swing'].forEach(updateLabel);
  updateSwatch();
  setOn('bg_power', true);
  queue(flowOn() ? flowParams() : { bg_power: 'on', hue: sc.h, sat: sc.s, bg_bright: BG_BRIGHT }, true);
}

$('scene_save').addEventListener('click', () => {
  const name = $('scene_name').value.trim().slice(0, 20);
  if (!name) { sceneNote('enter a scene name'); return; }
  const sc = { n: name, e: effect(), h: +$('hue').value, s: +$('sat').value, b: BG_BRIGHT,
               fs: flowSec, fm: flowMin, w: +$('swing').value };
  const i = scenes.findIndex(x => x.n === name);
  if (i >= 0) scenes[i] = sc;  // the same name: replace
  else if (scenes.length >= SCENE_MAX) { sceneNote('up to ' + SCENE_MAX + ' scenes can be stored, delete one you do not need'); return; }
  else scenes.push(sc);
  $('scene_name').value = '';
  renderScenes();
  saveScenes();
});

['bright', 'ct', 'hue', 'sat', 'flow_s', 'swing', 'sleep_m'].forEach(updateLabel);
loadVersion();
loadScenes();
renderApi();
renderApiNow();
refresh();
setInterval(() => { if (!stateLoaded) refresh(); }, 3000);  // until the lamp has replied, keep trying
setInterval(refresh, 15000);
setTimeout(loadSystem, 3000);  // let the lamp state be read first
setInterval(loadSystem, 5000);
</script>
</body>
</html>
)HTML";