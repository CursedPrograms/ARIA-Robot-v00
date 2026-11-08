// ARIA's controller page: reads /status every second, and says the fleet's phrases through /chirp.
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

  // Her two talk switches (/talkmute; RIFT has the same): her LED when she chats, and the chatting itself
  var talkBtn = document.getElementById("talk"), chatBtn = document.getElementById("chat");
  function showTalk(s) {
    talkBtn.textContent = s.talk_muted ? "LED talk: off" : "LED talk: on";
    chatBtn.textContent = s.chat_off ? "Chat: off" : "Chat: on";
  }
  function flip(which) {
    fetch("/talkmute?" + which + "=toggle").then(function (r) { return r.json(); })
      .then(showTalk).catch(function () {});
  }
  talkBtn.addEventListener("click", function () { flip("on"); });
  chatBtn.addEventListener("click", function () { flip("chat_off"); });

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
      box.querySelectorAll("button").forEach(function (b) { b.disabled = s.flying || !!s.chat_off; });
      showTalk(s);
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
