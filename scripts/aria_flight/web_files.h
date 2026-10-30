// Made by scripts/embed_web.py from web/: edit those, not this.
#pragma once

static const char WEB_APP_JS[] = R"WEBFILE(// ARIA's controller page: reads /status every second, and says the fleet's phrases through /chirp.
(function () {
  var PHRASES = ["hello", "how are you", "happy", "curious", "sleepy", "let's play", "bye"];
  var box = document.getElementById("phrases");
  var note = document.getElementById("note");
  var link = document.getElementById("link");

  PHRASES.forEach(function (name, u) {
    var b = document.createElement("button");
    b.textContent = name;
    b.addEventListener("click", function () {
      fetch("/chirp?u=" + u).then(function (r) {
        return r.json();
      }).then(function (j) {
        note.textContent = j.ok ? "Saying " + name + "." : j.error;
      }).catch(function () {
        note.textContent = "She didn't answer.";
      });
    });
    box.appendChild(b);
  });

  function set(id, text) {
    document.getElementById(id).textContent = text;
  }

  function poll() {
    fetch("/status").then(function (r) {
      return r.json();
    }).then(function (s) {
      link.textContent = s.flying ? "flying" : "connected";
      link.className = "pill " + (s.flying ? "flying" : "on");
      set("mode", s.mode);
      set("battery", s.battery_v.toFixed(2) + " V");
      set("network", s.network || "her own WiFi");
      set("rssi", s.rssi ? s.rssi + " dBm" : "--");
      set("pitch", s.pitch.toFixed(1) + "\u00b0");
      set("roll", s.roll.toFixed(1) + "\u00b0");
      box.querySelectorAll("button").forEach(function (b) { b.disabled = s.flying; });
    }).catch(function () {
      link.textContent = "no answer";
      link.className = "pill";
    });
  }

  // Rough metres from radio strength: about 1 m at -50 dBm
  function metres(rssi) {
    var m = Math.pow(10, (-50 - rssi) / 25);
    return "~" + (m < 10 ? m.toFixed(1) : m.toFixed(0)) + " m";
  }

  function pollNear() {
    fetch("/near").then(function (r) {
      return r.json();
    }).then(function (j) {
      var near = document.getElementById("near");
      near.textContent = "";
      j.robots.forEach(function (r) {
        var tile = document.createElement("div"), name = document.createElement("span"), far = document.createElement("strong");
        tile.className = "tile";
        name.textContent = r.name;
        far.textContent = metres(r.rssi);
        tile.appendChild(name);
        tile.appendChild(far);
        near.appendChild(tile);
      });
      if (!j.robots.length) near.textContent = "Nobody in range.";
    }).catch(function () {});
  }

  poll();
  pollNear();
  setInterval(poll, 1000);
  setInterval(pollNear, 2000);
})();
)WEBFILE";

static const char WEB_FLY_CSS[] = R"WEBFILE(/* Flying ARIA from a browser: cyan sky, white clouds, two sticks */
:root {
  --bg: #E6FAFD;
  --panel: #FFFFFF;
  --border: #B5EAF2;
  --accent: #0891B2;
  --bright: #22D3EE;
  --text: #0B3A44;
  --stop: #B91C1C;
}
* { box-sizing: border-box; }
html, body { height: 100%; margin: 0; overflow: hidden; touch-action: none; user-select: none; -webkit-user-select: none; }
body {
  display: flex;
  flex-direction: column;
  background: var(--bg);
  color: var(--text);
  font: 15px system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
}
header { display: flex; align-items: center; gap: 12px; padding: 6px 12px; background: var(--panel); border-bottom: 1px solid var(--border); }
.name { color: var(--accent); font-weight: 700; letter-spacing: 4px; text-decoration: none; }
#state { flex: 1; }
label { display: flex; align-items: center; gap: 6px; }
button { padding: 8px 14px; border: 0; border-radius: 8px; color: #FFFFFF; font: inherit; font-weight: 700; touch-action: none; }
#arm { background: var(--accent); }
#arm.armed { background: var(--bright); color: var(--text); }
#stop { background: var(--stop); padding: 8px 22px; }
main { flex: 1; display: flex; justify-content: space-around; align-items: center; padding: 10px; min-height: 0; }
.stick { position: relative; height: 100%; aspect-ratio: 1; max-width: 46%; background: var(--panel); border: 2px solid var(--border); border-radius: 18px; touch-action: none; }
.stick span { position: absolute; left: 0; right: 0; bottom: 6px; text-align: center; font-size: 12px; pointer-events: none; }
.knob { position: absolute; left: 50%; top: 50%; width: 22%; height: 22%; margin: -11% 0 0 -11%; border-radius: 50%; background: var(--border); pointer-events: none; }
.stick.held .knob { background: var(--bright); }
)WEBFILE";

static const char WEB_FLY_HTML[] = R"WEBFILE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Fly ARIA</title>
<meta name="color-scheme" content="light">
<link rel="stylesheet" href="/fly.css">
<script src="/fly.js" defer></script>
</head>
<body>
<header>
  <a class="name" href="/">ARIA</a>
  <span id="state">connecting...</span>
  <label>Limit <span id="limitValue">50%</span>
    <input type="range" id="limit" min="20" max="80" value="50">
  </label>
  <button id="arm">HOLD TO ARM</button>
  <button id="stop">STOP</button>
</header>
<main>
  <div class="stick" id="left"><div class="knob"></div><span>throttle + turn: lift off to power down</span></div>
  <div class="stick" id="right"><div class="knob"></div><span>tilt</span></div>
</main>
</body>
</html>
)WEBFILE";

static const char WEB_FLY_JS[] = R"WEBFILE(// Flying ARIA from a browser: the same rules as her Android app (AriaProtocol.kt), over a WebSocket.
// Her firmware only takes this from her own WiFi, caps it again, and brings her down if it stops.
var AriaFlight = (function () {
  var MAX_TILT = 15, MAX_YAW = 120, FULL = 60000, CLIMB_S = 1, DROP_S = 0.5, ARM_THROTTLE = 0.05;

  function clamp(v, lo, hi) { return Math.min(hi, Math.max(lo, v)); }

  function Flight() {
    this.armed = false;
    this.limit = 0.5;
    this.thrust = 0;
  }
  // Only with the throttle down
  Flight.prototype.arm = function (throttle) {
    if ((throttle || 0) > ARM_THROTTLE) return false;
    this.armed = true;
    this.thrust = 0;
    return true;
  };
  Flight.prototype.stop = function () {
    this.armed = false;
    this.thrust = 0;
  };
  // sticks: {throttle: 0..1 or null with no finger on it, yaw, roll, pitch: -1..1}
  Flight.prototype.tick = function (seconds, s) {
    if (!this.armed) return { roll: 0, pitch: 0, yaw: 0, thrust: 0 };
    var want = clamp(s.throttle || 0, 0, 1) * this.limit;
    this.thrust = want > this.thrust ? Math.min(want, this.thrust + seconds / CLIMB_S)
                                     : Math.max(want, this.thrust - seconds / DROP_S);
    return { roll: clamp(s.roll, -1, 1) * MAX_TILT, pitch: clamp(s.pitch, -1, 1) * MAX_TILT,
             yaw: clamp(s.yaw, -1, 1) * MAX_YAW, thrust: Math.floor(this.thrust * FULL) };
  };
  // Roll, wire pitch (forward is negative), yaw as little-endian floats, then thrust
  function packet(c) {
    var v = new DataView(new ArrayBuffer(14));
    v.setFloat32(0, c.roll, true);
    v.setFloat32(4, -c.pitch, true);
    v.setFloat32(8, c.yaw, true);
    v.setUint16(12, c.thrust, true);
    return v.buffer;
  }
  return { Flight: Flight, packet: packet };
})();

if (typeof module !== "undefined") {
  module.exports = AriaFlight;       // node, for her tests
} else {
  (function () {
    var flight = new AriaFlight.Flight();
    var sticks = { throttle: null, yaw: 0, roll: 0, pitch: 0 };
    var state = document.getElementById("state");
    var armBtn = document.getElementById("arm");
    var socket = null, armTimer = 0, last = performance.now();

    function disarm() {
      flight.stop();
      armBtn.textContent = "HOLD TO ARM";
      armBtn.className = "";
    }

    function connect() {
      socket = new WebSocket("ws://" + location.host + "/fly-ws");
      socket.binaryType = "arraybuffer";
      socket.onopen = function () { state.textContent = "connected"; };
      socket.onclose = function () {
        disarm();
        state.textContent = "Not connected. Flying only works on her own WiFi.";
        setTimeout(connect, 2000);
      };
    }

    function stick(id, onMove, onLift) {
      var el = document.getElementById(id), knob = el.querySelector(".knob"), finger = null;
      function move(e) {
        var r = el.getBoundingClientRect();
        var x = Math.min(1, Math.max(-1, (e.clientX - r.left) / r.width * 2 - 1));
        var y = Math.min(1, Math.max(-1, (e.clientY - r.top) / r.height * 2 - 1));
        knob.style.transform = "translate(" + x * r.width / 2 + "px," + y * r.height / 2 + "px)";
        onMove(x, y);
      }
      function lift(e) {
        if (e.pointerId !== finger) return;
        finger = null;
        el.classList.remove("held");
        knob.style.transform = "";
        onLift();
      }
      el.addEventListener("pointerdown", function (e) {
        finger = e.pointerId;
        el.setPointerCapture(finger);
        el.classList.add("held");
        move(e);
      });
      el.addEventListener("pointermove", function (e) { if (e.pointerId === finger) move(e); });
      el.addEventListener("pointerup", lift);
      el.addEventListener("pointercancel", lift);
    }

    stick("left", function (x, y) { sticks.yaw = x; sticks.throttle = (1 - y) / 2; },
          function () { sticks.yaw = 0; sticks.throttle = null; });
    stick("right", function (x, y) { sticks.roll = x; sticks.pitch = -y; },
          function () { sticks.roll = 0; sticks.pitch = 0; });

    // A second's hold, so a stray thumb can't arm her
    armBtn.addEventListener("pointerdown", function () {
      armTimer = setTimeout(function () {
        if (flight.arm(sticks.throttle)) {
          armBtn.textContent = "ARMED";
          armBtn.className = "armed";
        } else {
          state.textContent = "Throttle down first";
        }
      }, 1000);
    });
    armBtn.addEventListener("pointerup", function () { clearTimeout(armTimer); });
    armBtn.addEventListener("pointercancel", function () { clearTimeout(armTimer); });
    document.getElementById("stop").addEventListener("pointerdown", disarm);

    var limit = document.getElementById("limit");
    limit.addEventListener("input", function () {
      flight.limit = limit.value / 100;
      document.getElementById("limitValue").textContent = limit.value + "%";
    });

    // Switching tab or app: disarmed, and timers stop, so this can't be left running
    document.addEventListener("visibilitychange", function () { if (document.hidden) disarm(); });
    window.addEventListener("pagehide", disarm);

    setInterval(function () {
      var now = performance.now(), c = flight.tick((now - last) / 1000, sticks);
      last = now;
      if (socket && socket.readyState === 1) socket.send(AriaFlight.packet(c));
    }, 33);

    connect();
  })();
}
)WEBFILE";

static const char WEB_INDEX_HTML[] = R"WEBFILE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ARIA</title>
<meta name="color-scheme" content="light">
<link rel="stylesheet" href="/style.css">
<script src="/app.js" defer></script>
</head>
<body>
<header>
  <h1>ARIA</h1>
  <p>Aerial Roaming Intelligent Automaton</p>
</header>

<main>
  <section class="state">
    <div id="link" class="pill">looking for her...</div>
    <div class="tiles">
      <div class="tile"><span>State</span><strong id="mode">--</strong></div>
      <div class="tile"><span>Battery</span><strong id="battery">--</strong></div>
      <div class="tile"><span>Network</span><strong id="network">--</strong></div>
      <div class="tile"><span>Signal</span><strong id="rssi">--</strong></div>
      <div class="tile"><span>Pitch</span><strong id="pitch">--</strong></div>
      <div class="tile"><span>Roll</span><strong id="roll">--</strong></div>
    </div>
  </section>

  <section>
    <h2>Robots nearby</h2>
    <p class="hint">Rough distances from radio strength.</p>
    <div class="tiles" id="near"></div>
  </section>

  <section>
    <h2>Say something</h2>
    <p class="hint">In Brainfuck, on the ground only.</p>
    <div class="buttons" id="phrases"></div>
    <div id="note" class="hint"></div>
  </section>

  <section>
    <h2>Flying her</h2>
    <p class="hint">On her own WiFi: <a href="/fly">fly her from this browser</a>, or use her app, the ESP-Drone app or a gamepad. The fleet's network can't fly her.</p>
  </section>
</main>
</body>
</html>
)WEBFILE";

static const char WEB_STYLE_CSS[] = R"WEBFILE(/* ARIA's controller page: cyan sky, white clouds */
:root {
  --bg: #E6FAFD;
  --panel: #FFFFFF;
  --border: #B5EAF2;
  --accent: #0891B2;
  --bright: #22D3EE;
  --text: #0B3A44;
  --sub: #3E6E79;
  --warn: #B45309;
}
* { box-sizing: border-box; }
body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 16px/1.5 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif;
}
header { text-align: center; padding: 32px 16px 12px; }
h1 { margin: 0; color: var(--accent); font-size: 48px; letter-spacing: 8px; }
header p { margin: 4px 0 0; color: var(--sub); letter-spacing: 2px; font-size: 13px; text-transform: uppercase; }
main { max-width: 560px; margin: 0 auto; padding: 8px 16px 32px; }
section { background: var(--panel); border: 1px solid var(--border); border-radius: 14px; padding: 16px; margin-top: 14px; }
h2 { margin: 0 0 4px; color: var(--accent); font-size: 14px; letter-spacing: 2px; text-transform: uppercase; }
.hint { margin: 0 0 10px; color: var(--sub); font-size: 13px; }
.pill { display: inline-block; padding: 3px 12px; border-radius: 99px; background: var(--bg); color: var(--sub); font-size: 13px; }
.pill.on { background: var(--bright); color: var(--text); }
.pill.flying { background: var(--warn); color: #FFFFFF; }
.tiles { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; margin-top: 12px; }
.tile { background: var(--bg); border-radius: 10px; padding: 8px 12px; }
.tile span { display: block; color: var(--sub); font-size: 12px; }
.tile strong { font-size: 18px; overflow-wrap: anywhere; }
.buttons { display: flex; flex-wrap: wrap; gap: 8px; }
button {
  flex: 1 1 30%;
  padding: 10px;
  border: 1px solid var(--accent);
  border-radius: 10px;
  background: var(--panel);
  color: var(--accent);
  font: inherit;
  cursor: pointer;
}
button:hover { background: var(--bg); }
button:disabled { border-color: var(--border); color: var(--border); cursor: default; }
#note { margin: 10px 0 0; min-height: 20px; }
)WEBFILE";

