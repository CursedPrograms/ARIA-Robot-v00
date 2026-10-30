// Flying ARIA from a browser: the same rules as her Android app (AriaProtocol.kt), over a WebSocket.
// Her firmware only takes this from her own WiFi, caps it again, and cuts the motors if it goes quiet.
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
