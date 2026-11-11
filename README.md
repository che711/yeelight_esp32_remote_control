# Web remote for the Yeelight Screen Light Bar Pro (YLTD003)

An ESP32-C6-Zero board joins your Wi-Fi and controls the lamp over the local network. Open the board's address in a browser on your phone or computer and you get a remote: the main light, the colored backlight, effects and scenes. The same actions are available from a terminal with `curl`.

The project exists because the original remote was lost. The Yeelight cloud is not needed: the board talks to the lamp directly.

## Features

- **Main light:** brightness, color temperature, on and off. Turning it off keeps the backlight lit (the page sets the backlight again right after the lamp turns both lights off).
- **Backlight:** color, saturation, a "White" button in the Effect block and "Lock color" (stops an effect on the current color). The backlight brightness is always 100%.
- **Effects:** Wave, Campfire, Candle, Sunset, Sunrise, Storm, Ocean, Lava, Heartbeat, Garland, Police.
- **Color cycling:** Rainbow, Warm, Cold, Neon, Pastel, with a "Brightness swing" slider.
- **Sleep timer:** the backlight fades out over the chosen time (up to 120 minutes on the page, up to 240 through the API).
- **Scenes:** up to 8 saved backlight settings, stored in the board's memory and kept across reboots.
- **System block:** firmware version and build date, chip, memory, server load, temperature, Wi-Fi, IP, board reboot. The version is also shown next to the page title.
- **Terminal API block:** ready-made `curl` commands with a copy button.

The effects run on the lamp itself; the board only starts them. At any moment the whole backlight shows one color: the lamp cannot show different colors along its length.

## What you need

- A Waveshare ESP32-C6-Zero board.
- A Yeelight Screen Light Bar Pro on the same Wi-Fi network.
- LAN Control enabled for the lamp in the Yeelight app.
- [PlatformIO](https://platformio.org/) (pioarduino platform, Arduino framework).
- For the tests and checks: Python 3 and Node.js.

## Setup and build

1. Create `src/config.h`. Do not commit it, it holds your Wi-Fi password:

   ```cpp
   #pragma once
   #define LAMP_IP       "192.168.1.60"   // the lamp's address on your network
   #define LAMP_PORT     55443            // the lamp's control port
   #define WIFI_SSID     "your-network"
   #define WIFI_PASS     "your-password"
   #define WIFI_HOSTNAME "lamp-remote"    // how the board appears in the router's list
   ```

2. `platformio.ini` must have: the board `esp32-c6-devkitc-1`, the flags `-DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1`, monitor speed 115200 and upload speed 460800.

3. Build and flash:

   ```
   pio run                      # build
   pio run -t upload            # build and upload to the board
   pio device monitor           # serial monitor
   ```

4. The monitor prints the board's address: `Page: http://<address>`. Open it in a browser. The page itself is in English.

The Wi-Fi power is limited to 8.5 dBm: on this board the radio does not work at full power.

## Using it from a terminal

There is no password: anyone on your network can control the lamp. Below, `<address>` is the board's address.

```
curl <address>/api/state                                          # lamp state
curl "<address>/api/set?main=dim"                                 # dim the main light
curl "<address>/api/set?bg_power=on&hue=0&sat=100&bg_bright=80"   # red backlight
curl "<address>/api/set?bg_flow=fire&flow_ms=2000&bg_bright=80"   # campfire
curl "<address>/api/set?bg_flow=sunset&flow_ms=900000"            # sunset over 15 minutes
curl "<address>/api/set?bg_sleep=30&hue=30&sat=80&bg_bright=60"   # sleep timer for 30 minutes
curl "<address>/api/set?power=off"                                # turn everything off
curl -G "<address>/api/scene" --data-urlencode "name=Evening"     # start a scene
```

### `/api/set` parameters

Parameters can be combined with `&`.

| Parameter | Value |
|---|---|
| `power` | `on`, `off`: both lights at once |
| `main` | `dim`, `restore`: dim the main light to 1% or return its brightness (`main_save` remembers the current one) |
| `bright`, `ct` | main light brightness 1-100, temperature 2700-6500 K |
| `bg_power` | `on`, `off`: the backlight |
| `hue`, `sat`, `bg_bright` | color 0-359, saturation 0-100, backlight brightness 1-100 (the page always sends 100) |
| `bg_check` | `0` with `bg_power=on`: skip the check that the backlight really turned on (saves a command) |
| `bg_flow` | `on` (rainbow), `warm`, `cold`, `neon`, `pastel`, `wave`, `fire`, `candle`, `sunset`, `sunrise`, `storm`, `ocean`, `lava`, `heart`, `garland`, `police`, `off` |
| `flow_ms` | time of one step in milliseconds; for `sunset` and `sunrise` the total duration |
| `swing` | brightness swing of color cycling, 0-90 |
| `bg_sleep` | sleep timer in minutes, 1-240 |

`wave` and `heart` take their color from `hue` and `sat`.

### Other addresses

| Address | What it does |
|---|---|
| `GET /` | the remote page |
| `GET /api/state` | lamp state and the last effect that was started |
| `GET /api/scene?name=...` | start a saved scene (404 if there is none; names with quotes and slashes are not found) |
| `GET`, `POST /api/scenes` | read or write the whole list of scenes |
| `GET /api/system` | board information, including the firmware version and build date |
| `POST /api/reboot` | reboot the board |

## Lamp quirks

The remote is built the way it is because of these:

- The lamp accepts about 60 commands per minute, otherwise it goes silent. The board keeps at least 0.3 s between commands and sends no more than 50 commands in any 60 seconds (if the limit is reached it waits for up to 10 seconds, then skips the command). The page sends slider commands no more than once per 1.2 s.
- The `bg_set_power on` command replies "ok" but does not turn the backlight on. So the backlight is turned on with a `bg_set_scene` scene; then the board checks `bg_power` and, if needed, switches it with `bg_toggle`.
- There is no command to turn off only the main light: `set_power` and `toggle` act on both lights. So the page turns both lights off and, in the same request, sets the backlight again. The backlight blinks for a moment, a running effect starts over and a sleep timer is restarted with the time left. The API still has `main=dim`, which dims the main light to 1%.
- The `power` property means "the main light or the backlight is on". The state of the main light is reported by `main_power`.

## Tests

Checks without a board (they also run in CI):

```
python3 tests/check_static.py
```

The board tests use a fake lamp, `tests/fake_lamp.py`: the test sends requests to the board, and the fake lamp records which commands arrived and watches the lamp's rules (the pause between commands, the 60 per minute limit, the shape of color chains).

1. In `config.h` set `LAMP_IP` to this computer's address and flash the board.
2. Run:

   ```
   pip install pytest
   BOARD_URL=http://<board address> pytest -v tests
   ```

   If the lamp port is not 55443, add `LAMP_PORT=<port>`.

The scene tests temporarily overwrite the scenes on the board and restore the previous ones at the end. After the tests put the real lamp address back into `config.h`.

You can see which commands the board sends to the lamp without the tests too: `python3 tests/fake_lamp.py`.

## GitHub Actions

The workflow `.github/workflows/ci.yml` runs on every push and pull request:

- runs `tests/check_static.py` and `pytest` (the board tests are skipped without a board);
- builds the firmware with PlatformIO and saves `firmware.bin` as a run artifact. If `config.h` is not in the repository, a placeholder is created for the build.

## Project structure

```
├── platformio.ini
├── .github/workflows/ci.yml   checks and firmware build
├── src/
│   ├── main.cpp               firmware: lamp connection, API, effects, scenes
│   ├── index_html.h           the web page as one string (HTML, CSS, JS)
│   └── config.h               your Wi-Fi and lamp settings (not committed)
└── tests/
    ├── fake_lamp.py           the fake lamp
    ├── test_api.py            board tests
    └── check_static.py        checks without a board
```

## Adding an effect

1. In `src/main.cpp` add a `FlowStep` array and a row in the `FLOW_DEFS` table.
2. In `src/index_html.h` add a row to the `EFFECTS` table. The button appears by itself. If you want, add the button strip animation in CSS.
3. Run `python3 tests/check_static.py`: it checks that the lists match.

## Firmware version

The version is the `FW_VERSION` define at the top of `src/main.cpp` (format `x.y.z`). Change it with every release: it is shown on the page next to the title and in the System block, together with the build date. You can also pass it from the build: `-DFW_VERSION=\"1.2.0\"` in `build_flags`.

## Known limitations

- There is no password on the page or the API.
- After the board reboots, a running effect is shown on the page as "Rainbow": the lamp does not say which effect is running.
- Sunset turns the backlight off at the end, sunrise stays on the last color. The sleep timer fades in the color set on the sliders.
- Not verified on a real lamp: long effect chains (campfire, storm, garland, police, lava, ocean), long sunset and sunrise durations (up to 30 minutes), the sleep timer up to 120 minutes, the `/api/scene` reply, and the `bg_flowing`, `bg_rgb` and `main_power` properties in `get_prop`. If the lamp replies with anything other than "ok", send the lines from the serial monitor: the command (`->`) and the reply (`<-`). Every line carries the board's uptime in milliseconds, and the command line also shows how long it waited for its turn (`waited`), so you can see where the time goes.