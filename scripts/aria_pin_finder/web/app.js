// The pin finder's page: every button sends one command, the same ones as over serial.
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
