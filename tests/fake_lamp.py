"""A fake Yeelight lamp for testing the board without a real lamp.

It listens on a TCP port (55443 by default), replies the way the lamp does, records every command
and watches the lamp's rules:
  - one command per connection;
  - a pause between commands (the board keeps 0.3 s; the check allows a small margin for the network);
  - no more than 60 commands per minute;
  - color flow (cf) chains are well formed: four numbers per step, mode 1, brightness 1..100, color 0..0xFFFFFF.
Violations are collected in the violations list.

Manual run: python3 tests/fake_lamp.py  (then put this computer's address into LAMP_IP in config.h)
"""
import json
import socket
import socketserver
import threading
import time

MIN_GAP_S = 0.25       # the board waits 0.3 s; the other 0.05 s is for network delays
RATE_LIMIT = 60        # commands per minute
RATE_WINDOW_S = 59.0   # slightly less than a minute, to avoid false alarms


class Command:
    def __init__(self, t, method, params, raw):
        self.t = t
        self.method = method
        self.params = params
        self.raw = raw

    def __repr__(self):
        return "%s %s" % (self.method, json.dumps(self.params, ensure_ascii=False))


def check_flow(params):
    """Checks a bg_set_scene ["cf", count, action, "dur,mode,rgb,bright,..."] chain. Returns a list of errors."""
    errors = []
    if len(params) != 4:
        return ["cf: expected 4 parameters, got %d" % len(params)]
    _, count, action, flow = params
    if action not in (0, 1, 2):
        errors.append("cf: action %r (allowed: 0, 1, 2)" % (action,))
    if not isinstance(flow, str):
        return errors + ["cf: the chain is not a string"]
    try:
        nums = [int(x) for x in flow.split(",")]
    except ValueError:
        return errors + ["cf: the chain has non-numbers: %r" % flow]
    if len(nums) % 4:
        return errors + ["cf: the number of values %d is not divisible by 4" % len(nums)]
    steps = len(nums) // 4
    if count not in (0, steps):
        errors.append("cf: count=%r but there are %d steps (must be 0 or the number of steps)" % (count, steps))
    for i in range(steps):
        dur, mode, rgb, bright = nums[i * 4:i * 4 + 4]
        if dur < 50:
            errors.append("cf: step %d is shorter than 50 ms (%d)" % (i, dur))
        if mode != 1:
            errors.append("cf: step %d has mode %d (the board only uses 1)" % (i, mode))
        if not 0 <= rgb <= 0xFFFFFF:
            errors.append("cf: step %d color %d is out of range" % (i, rgb))
        if not 1 <= bright <= 100:
            errors.append("cf: step %d brightness %d is outside 1..100" % (i, bright))
    return errors


class FakeLamp:
    def __init__(self, host="0.0.0.0", port=55443):
        self.host = host
        self.port = port
        self.lock = threading.Lock()
        self.commands = []      # commands since the last clear()
        self.violations = []    # rule violations since the last clear()
        self._times = []        # times of all commands in the session, for the rate limit check
        self._last_t = None
        # Behavior that tests can change
        self.silent = False             # do not reply at all (like a lamp that hit its limit)
        self.bg_scene_keeps_off = False  # the first scene does not turn the backlight on (this happens with a real lamp)
        self.props_before_reply = True   # send a props notification before the reply
        self.reset_state()
        self._server = None
        self._thread = None

    def reset_state(self):
        self.state = {
            "power": "on", "main_power": "on", "bright": "70", "ct": "4000",
            "bg_power": "off", "bg_bright": "50", "bg_hue": "0", "bg_sat": "100",
            "bg_flowing": "0", "bg_rgb": "0",
        }

    # ----- control from tests -----

    def clear(self):
        with self.lock:
            self.commands = []
            self.violations = []

    def start(self):
        lamp = self

        class Handler(socketserver.StreamRequestHandler):
            def handle(self):
                lamp._handle(self)

        class Server(socketserver.ThreadingTCPServer):
            allow_reuse_address = True
            daemon_threads = True

        self._server = Server((self.host, self.port), Handler)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()

    def stop(self):
        if self._server:
            self._server.shutdown()
            self._server.server_close()

    def methods(self):
        with self.lock:
            return [c.method for c in self.commands]

    def find(self, method):
        with self.lock:
            return [c for c in self.commands if c.method == method]

    # ----- connection handling -----

    def _handle(self, h):
        h.request.settimeout(5)
        try:
            line = h.rfile.readline()
        except (socket.timeout, OSError):
            return
        if not line:
            return
        now = time.monotonic()
        try:
            msg = json.loads(line.decode("utf-8"))
            cid, method, params = msg["id"], msg["method"], msg["params"]
        except (ValueError, KeyError) as e:
            with self.lock:
                self.violations.append("cannot parse the command %r: %s" % (line, e))
            return

        with self.lock:
            self.commands.append(Command(now, method, params, line.decode("utf-8", "replace").strip()))
            self._check_rules(now, method, params)
            silent = self.silent
            result = None if silent else self._apply(method, params)

        if silent:
            time.sleep(4)  # the board waits 3 seconds for a reply and gives up
            return
        try:
            if self.props_before_reply and method != "get_prop":
                h.wfile.write(b'{"method":"props","params":{"power":"on"}}\r\n')
            h.wfile.write((json.dumps({"id": cid, "result": result}) + "\r\n").encode())
            h.wfile.flush()
        except OSError:
            pass
        # one command per connection: after this the lamp does not listen; the board closes it itself

    def _check_rules(self, now, method, params):
        if self._last_t is not None and now - self._last_t < MIN_GAP_S:
            self.violations.append("%.2f s pause before %s (the board must keep at least 0.3 s)" % (now - self._last_t, method))
        self._last_t = now
        self._times.append(now)
        if len(self._times) > RATE_LIMIT:
            span = now - self._times[-(RATE_LIMIT + 1)]
            if span < RATE_WINDOW_S:
                self.violations.append("more than %d commands in %.0f s" % (RATE_LIMIT, span))
        if method == "bg_set_scene" and params and params[0] == "cf":
            self.violations.extend(check_flow(params))

    # ----- lamp state -----

    def _apply(self, method, params):
        s = self.state
        if method == "get_prop":
            return [s.get(name, "") for name in params]
        if method == "set_power":
            s["power"] = s["main_power"] = params[0]
            if params[0] == "off":  # turns both lights off at once
                s["bg_power"] = "off"
                s["bg_flowing"] = "0"
        elif method == "set_bright":
            s["bright"] = str(params[0])
        elif method == "set_ct_abx":
            s["ct"] = str(params[0])
        elif method == "bg_set_power":
            # on a real lamp "on" replies ok but does nothing
            if params[0] == "off":
                s["bg_power"] = "off"
                s["bg_flowing"] = "0"
        elif method == "bg_set_scene":
            if self.bg_scene_keeps_off:
                self.bg_scene_keeps_off = False  # only the first time
            else:
                s["bg_power"] = "on"
            if params[0] == "hsv":
                s["bg_hue"], s["bg_sat"], s["bg_bright"] = str(params[1]), str(params[2]), str(params[3])
                s["bg_flowing"] = "0"
            elif params[0] == "cf":
                s["bg_flowing"] = "1"
        elif method == "bg_toggle":
            s["bg_power"] = "off" if s["bg_power"] == "on" else "on"
        elif method == "bg_stop_cf":
            s["bg_flowing"] = "0"
        elif method == "bg_set_hsv":
            s["bg_hue"], s["bg_sat"] = str(params[0]), str(params[1])
        elif method == "bg_set_bright":
            s["bg_bright"] = str(params[0])
        return ["ok"]


if __name__ == "__main__":
    lamp = FakeLamp()
    lamp.start()
    print("the fake lamp is listening on port %d; commands are printed below, Ctrl+C to quit" % lamp.port)
    seen = 0
    try:
        while True:
            time.sleep(0.2)
            with lamp.lock:
                new = lamp.commands[seen:]
                seen = len(lamp.commands)
                bad = list(lamp.violations)
                lamp.violations.clear()
            for c in new:
                print("->", c)
            for v in bad:
                print("!! VIOLATION:", v)
    except KeyboardInterrupt:
        lamp.stop()