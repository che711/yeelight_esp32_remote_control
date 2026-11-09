"""Board tests with a fake lamp.

How to run:
  1. In config.h set LAMP_IP to this computer's address (LAMP_PORT = 55443) and flash the board.
  2. pip install pytest
  3. BOARD_URL=http://<board address> pytest -v tests

The test sends requests to the board (/api/*), and the fake lamp (fake_lamp.py), which the test starts itself,
records which commands arrived. After each test it checks that the lamp's rules were not broken:
the pause between commands, the limit of 60 per minute, the shape of color flow chains.

The scene tests temporarily overwrite the scenes on the board and restore the previous ones at the end.
"""
import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

import pytest

from fake_lamp import FakeLamp

BOARD = os.environ.get("BOARD_URL", "").rstrip("/")
LAMP_PORT = int(os.environ.get("LAMP_PORT", "55443"))
ROOT = Path(__file__).resolve().parent.parent

pytestmark = pytest.mark.skipif(not BOARD, reason="BOARD_URL is not set (the board address, e.g. http://192.168.1.50)")


def board_effects():
    """Effect names from the FLOW_DEFS table in main.cpp plus the three built directly in applySet."""
    text = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
    names = re.findall(r'^\s*\{"(\w+)",\s+FLOW_', text, re.M)
    assert names, "the FLOW_DEFS table was not found in main.cpp"
    return names + ["on", "wave", "fire"]


EFFECTS = board_effects()


# ---------- helpers ----------

def api(path, params=None, method="GET", body=None, timeout=40):
    """Returns (status code, parsed JSON)."""
    url = BOARD + path
    if params:
        url += "?" + urllib.parse.urlencode(params)
    headers = {"Content-Type": "application/json"} if body is not None else {}
    req = urllib.request.Request(url, data=body, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read().decode("utf-8") or "null")


def chain(cmd):
    """Parses a cf chain into a list of steps (duration, color, brightness)."""
    nums = [int(x) for x in cmd.params[3].split(",")]
    return [(nums[i], nums[i + 2], nums[i + 3]) for i in range(0, len(nums), 4)]


def set_ok(params):
    status, j = api("/api/set", params)
    assert status == 200
    return j


# ---------- shared setup ----------

@pytest.fixture(scope="session")
def lamp_server():
    lamp = FakeLamp(port=LAMP_PORT)
    lamp.start()
    yield lamp
    lamp.stop()


@pytest.fixture(autouse=True)
def lamp(lamp_server):
    lamp_server.reset_state()
    lamp_server.silent = False
    lamp_server.bg_scene_keeps_off = False
    lamp_server.clear()
    yield lamp_server
    assert not lamp_server.violations, "the lamp's rules were broken:\n" + "\n".join(lamp_server.violations)


@pytest.fixture
def scenes_backup():
    _, original = api("/api/scenes")
    yield
    api("/api/scenes", method="POST", body=json.dumps(original, ensure_ascii=False).encode("utf-8"))


def save_scenes(scenes):
    # ensure_ascii=False: the page also stores names as they are, and the board looks them up by text
    body = json.dumps(scenes, ensure_ascii=False).encode("utf-8")
    status, j = api("/api/scenes", method="POST", body=body)
    assert status == 200 and j["ok"]


# ---------- page and state ----------

def test_page_and_effect_table_match_firmware(lamp):
    """The list of effects on the page matches the list in the firmware."""
    req = urllib.request.Request(BOARD + "/")
    with urllib.request.urlopen(req, timeout=20) as r:
        html = r.read().decode("utf-8")
    assert "<title>Lamp</title>" in html
    page_effects = set(re.findall(r"board:\s*'(\w+)'", html))
    assert page_effects == set(EFFECTS)
    assert lamp.methods() == []  # the page does not touch the lamp


def test_system_reports_version(lamp):
    status, j = api("/api/system")
    assert status == 200
    assert re.fullmatch(r"\d+\.\d+\.\d+", j["version"]), j["version"]
    assert j["build"]
    assert lamp.methods() == []  # board information does not touch the lamp


def test_state(lamp):
    status, j = api("/api/state")
    assert status == 200 and j["ok"] is True
    for key in ("power", "bright", "ct", "bg_power", "bg_bright", "hue", "sat", "bg_flow", "rgb",
                "effect", "flow_ms", "swing", "main_saved", "sleep_left"):
        assert key in j, key
    assert lamp.methods() == ["get_prop"]
    assert "main_power" in lamp.commands[0].params


# ---------- main light ----------

def test_main_dim_and_restore(lamp):
    assert set_ok({"main": "dim", "main_save": 70})["ok"]
    assert [c.method for c in lamp.commands] == ["set_bright"]
    assert lamp.commands[0].params[0] == 1
    lamp.clear()
    assert set_ok({"main": "restore"})["ok"]
    assert lamp.commands[0].method == "set_bright"
    assert lamp.commands[0].params[0] == 70


def test_bright_and_ct_are_clamped(lamp):
    assert set_ok({"bright": 500, "ct": 100})["ok"]
    assert lamp.methods() == ["set_bright", "set_ct_abx"]
    assert lamp.commands[0].params[0] == 100
    assert lamp.commands[1].params[0] == 2700


def test_all_off(lamp):
    assert set_ok({"power": "off"})["ok"]
    assert lamp.methods() == ["set_power"]
    assert lamp.commands[0].params[0] == "off"


def test_main_off_brings_backlight_back(lamp):
    """What the page sends to turn off only the main light while the backlight is on a plain color."""
    assert set_ok({"power": "off", "bg_power": "on", "hue": 10, "sat": 100, "bg_bright": 50})["ok"]
    assert lamp.methods() == ["set_power", "bg_set_scene", "get_prop"]
    assert lamp.state["main_power"] == "off"
    assert lamp.state["bg_power"] == "on"


def test_main_off_brings_effect_back(lamp):
    """The same while an effect is running: the effect starts again in the same request."""
    assert set_ok({"power": "off", "bg_flow": "neon", "flow_ms": 3000, "bg_bright": 80, "swing": 0})["ok"]
    assert lamp.methods() == ["set_power", "bg_set_scene"]
    assert lamp.commands[1].params[0] == "cf"
    assert lamp.state["main_power"] == "off"
    assert lamp.state["bg_power"] == "on"


def test_main_off_restarts_sleep_timer(lamp):
    assert set_ok({"power": "off", "bg_sleep": 12, "hue": 30, "sat": 80, "bg_bright": 60})["ok"]
    assert lamp.methods() == ["set_power", "bg_set_scene"]
    assert chain(lamp.commands[1])[1][0] == 12 * 60000
    assert lamp.state["main_power"] == "off"


# ---------- backlight ----------

def test_bg_on_uses_scene_then_checks(lamp):
    assert set_ok({"bg_power": "on", "hue": 120, "sat": 90, "bg_bright": 60})["ok"]
    assert lamp.methods() == ["bg_set_scene", "get_prop"]
    assert lamp.commands[0].params == ["hsv", 120, 90, 60]
    assert lamp.state["bg_power"] == "on"


def test_bg_on_falls_back_to_toggle(lamp):
    lamp.bg_scene_keeps_off = True
    assert set_ok({"bg_power": "on", "hue": 0, "sat": 100, "bg_bright": 80})["ok"]
    assert lamp.methods() == ["bg_set_scene", "get_prop", "bg_toggle"]
    assert lamp.state["bg_power"] == "on"


def test_bg_off(lamp):
    assert set_ok({"bg_power": "off"})["ok"]
    assert lamp.methods() == ["bg_set_power"]
    assert lamp.commands[0].params[0] == "off"


def test_stop_effect_alone_and_with_color(lamp):
    assert set_ok({"bg_flow": "off"})["ok"]
    assert lamp.methods() == ["bg_stop_cf"]
    lamp.clear()
    # leaving an effect straight into a color: one command instead of two
    assert set_ok({"bg_flow": "off", "hue": 10, "sat": 50, "bg_bright": 40})["ok"]
    assert lamp.methods() == ["bg_set_scene"]
    assert lamp.commands[0].params == ["hsv", 10, 50, 40]


def test_plain_color_change_without_scene(lamp):
    assert set_ok({"hue": 200, "sat": 70})["ok"]
    assert lamp.methods() == ["bg_set_hsv"]


# ---------- effects ----------

@pytest.mark.parametrize("name", EFFECTS)
def test_effect_is_one_valid_command(lamp, name):
    j = set_ok({"bg_flow": name, "flow_ms": 3000, "bg_bright": 80, "hue": 200, "sat": 100, "swing": 20})
    assert j["ok"]
    assert lamp.methods() == ["bg_set_scene"]
    assert lamp.commands[0].params[0] == "cf"
    # the chain shape is checked by the lamp itself (violations are caught by the lamp fixture); here we check that the board remembers the effect
    lamp.clear()
    status, state = api("/api/state")
    assert state["effect"] == name


def test_sunset_runs_once_and_turns_off(lamp):
    assert set_ok({"bg_flow": "sunset", "flow_ms": 600000, "bg_bright": 70})["ok"]
    cmd = lamp.commands[0]
    assert cmd.params[1] == 6 and cmd.params[2] == 2  # six steps, then turn off
    assert sum(d for d, _, _ in chain(cmd)) == 600000


def test_sunrise_runs_once_and_stays(lamp):
    assert set_ok({"bg_flow": "sunrise", "flow_ms": 600000, "bg_bright": 70})["ok"]
    cmd = lamp.commands[0]
    assert cmd.params[1] == 6 and cmd.params[2] == 1  # six steps, then stay


def test_flashing_effects_are_not_too_fast(lamp):
    set_ok({"bg_flow": "police", "flow_ms": 500, "bg_bright": 100})
    assert min(d for d, _, _ in chain(lamp.commands[0])) >= 150
    lamp.clear()
    set_ok({"bg_flow": "storm", "flow_ms": 500, "bg_bright": 100})
    assert min(d for d, _, _ in chain(lamp.commands[0])) >= 80


def test_palette_swing_dims_every_second_color(lamp):
    set_ok({"bg_flow": "warm", "flow_ms": 3000, "bg_bright": 100, "swing": 50})
    steps = chain(lamp.commands[0])
    assert [b for _, _, b in steps] == [100, 50, 100, 50, 100, 50]


# ---------- sleep timer ----------

def test_sleep_timer(lamp):
    assert set_ok({"bg_sleep": 30, "hue": 30, "sat": 80, "bg_bright": 60})["ok"]
    assert lamp.methods() == ["bg_set_scene"]
    cmd = lamp.commands[0]
    assert cmd.params[1] == 2 and cmd.params[2] == 2  # two steps, then turn off
    steps = chain(cmd)
    assert steps[1][0] == 30 * 60000 and steps[1][2] == 1
    lamp.clear()
    _, state = api("/api/state")
    assert state["effect"] == "sleep"
    assert 0 < state["sleep_left"] <= 1800


# ---------- scenes ----------

def test_scene_with_effect(lamp, scenes_backup):
    save_scenes([{"n": "Test", "e": "neon", "h": 0, "s": 100, "b": 100, "fs": 4, "fm": 10, "w": 30}])
    lamp.clear()
    status, j = api("/api/scene", {"name": "Test"})
    assert status == 200 and j["ok"]
    steps = chain(lamp.commands[0])
    assert all(d == 4000 for d, _, _ in steps)
    assert [b for _, _, b in steps] == [100, 70, 100, 70, 100, 70]  # swing 30%


def test_scene_timed_effect_uses_minutes(lamp, scenes_backup):
    save_scenes([{"n": "Sunset15", "e": "sunset", "h": 0, "s": 100, "b": 100, "fs": 3, "fm": 15, "w": 0}])
    lamp.clear()
    status, j = api("/api/scene", {"name": "Sunset15"})
    assert status == 200 and j["ok"]
    assert sum(d for d, _, _ in chain(lamp.commands[0])) == 15 * 60000


def test_scene_plain_color(lamp, scenes_backup):
    save_scenes([{"n": "Blue", "e": "off", "h": 220, "s": 90, "b": 55, "fs": 3, "fm": 10, "w": 0}])
    lamp.clear()
    status, j = api("/api/scene", {"name": "Blue"})
    assert status == 200 and j["ok"]
    # the backlight brightness is always 100%: the 55 stored in an old scene is ignored
    assert lamp.commands[0].params == ["hsv", 220, 90, 100]


def test_scene_not_found(lamp):
    status, j = api("/api/scene", {"name": "no such scene"})
    assert status == 404 and j["ok"] is False
    assert lamp.methods() == []


# ---------- lamp rules ----------

def test_pause_between_commands(lamp):
    for v in (10, 20, 30, 40, 50):
        assert set_ok({"bright": v})["ok"]
    times = [c.t for c in lamp.commands]
    assert len(times) == 5
    gaps = [b - a for a, b in zip(times, times[1:])]
    assert min(gaps) >= 0.9, gaps


def test_silent_lamp_stops_the_chain(lamp):
    lamp.silent = True
    j = set_ok({"power": "on", "bright": 50, "ct": 4000})
    assert j["ok"] is False
    assert lamp.methods() == ["set_power"]  # the remaining commands are not tried
    