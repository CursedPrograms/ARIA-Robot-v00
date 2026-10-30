// Made by scripts/embed_web.py from web/: edit those, not this.
#pragma once

static const char WEB_APP_JS[] = R"WEBFILE(// The pin finder's page: every button sends one command, the same ones as over serial.
(function () {
  var log = document.getElementById("log");

  function send(cmd) {
    fetch("/cmd?c=" + encodeURIComponent(cmd))
      .then(function (r) { return r.text(); })
      .then(function (t) { log.textContent = t; })
      .catch(function () { log.textContent = "No answer from her"; });
  }

  document.querySelectorAll("button[data-cmd]").forEach(function (b) {
    b.addEventListener("click", function () { send(b.dataset.cmd); });
  });

  fetch("/pins").then(function (r) { return r.json(); }).then(function (pins) {
    var row = document.getElementById("pins");
    pins.forEach(function (pin) {
      var b = document.createElement("button");
      b.textContent = pin;
      b.addEventListener("click", function () { send("p " + pin); });
      row.appendChild(b);
    });
  });

  document.getElementById("spi").addEventListener("submit", function (e) {
    e.preventDefault();
    var f = e.target;
    send(["s", f.sck.value, f.miso.value, f.mosi.value, f.cs.value].join(" "));
  });

  // The walk moves on by itself, so keep showing where it's got to
  setInterval(function () {
    fetch("/last").then(function (r) { return r.text(); }).then(function (t) { log.textContent = t; }).catch(function () {});
  }, 1000);

  send("i");
})();
)WEBFILE";

static const char WEB_INDEX_HTML[] = R"WEBFILE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ARIA pin finder</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<h1>ARIA pin finder</h1>
<p class="warn">Propellers off before pressing anything.</p>
<section class="row">
  <button data-cmd="i">Chip</button>
  <button data-cmd="g">Find gyro</button>
  <button data-cmd="a">Find battery</button>
  <button data-cmd="w">Walk all pins</button>
  <button data-cmd="x" class="stop">STOP</button>
</section>
<h2>Pulse one pin</h2>
<section class="row" id="pins"></section>
<h2>Try gyro pins</h2>
<form id="spi" class="row">
  <input name="sck" placeholder="SCK" inputmode="numeric">
  <input name="miso" placeholder="MISO" inputmode="numeric">
  <input name="mosi" placeholder="MOSI" inputmode="numeric">
  <input name="cs" placeholder="CS" inputmode="numeric">
  <button>Try</button>
</form>
<pre id="log">...</pre>
<script src="/app.js"></script>
</body>
</html>
)WEBFILE";

static const char WEB_STYLE_CSS[] = R"WEBFILE(body { margin: 0; padding: 16px; background: #E6FAFD; color: #0B3A44; font-family: system-ui, sans-serif; }
h1 { margin: 0 0 4px; color: #0891B2; }
h2 { margin: 18px 0 6px; font-size: 1rem; }
.warn { margin: 0 0 12px; color: #B91C1C; font-weight: 700; }
.row { display: flex; flex-wrap: wrap; gap: 8px; }
button { padding: 10px 14px; border: 1px solid #B5EAF2; border-radius: 8px; background: #fff; color: #0B3A44; font: inherit; }
button:active { background: #22D3EE; }
button.stop { background: #B91C1C; border-color: #B91C1C; color: #fff; font-weight: 700; }
input { width: 64px; padding: 10px; border: 1px solid #B5EAF2; border-radius: 8px; font: inherit; }
pre { margin-top: 18px; padding: 12px; min-height: 90px; background: #fff; border: 1px solid #B5EAF2; border-radius: 8px; white-space: pre-wrap; }
)WEBFILE";

