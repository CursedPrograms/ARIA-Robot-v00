"""fake_aria.py - a pretend ARIA on this PC, for trying her page and RIFT's link with no drone.

Answers what aria_fleet.c answers on :5014 and serves the same web files:
    python scripts/fake_aria.py [--register http://127.0.0.1:5000] [--flying]
"""
import argparse
import base64
import hashlib
import json
import os
import random
import struct
import threading
import time
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

PORT = 5014
WEB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "esp-drone", "components", "drivers", "general", "wifi", "web")
TYPES = {"/": ("index.html", "text/html"), "/style.css": ("style.css", "text/css"), "/app.js": ("app.js", "text/javascript"),
         "/fly": ("fly.html", "text/html"), "/fly.css": ("fly.css", "text/css"), "/fly.js": ("fly.js", "text/javascript")}
state = {"flying": False}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def reply(self, code, body, kind="application/json"):
        body = body if isinstance(body, bytes) else body.encode()
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def fly_socket(self):
        """The browser's flight link: prints what she's told. (She only takes it on her own WiFi.)"""
        key = self.headers["Sec-WebSocket-Key"] + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", base64.b64encode(hashlib.sha1(key.encode()).digest()).decode())
        self.end_headers()
        self.close_connection = True
        while True:
            head = self.rfile.read(2)
            if len(head) < 2 or head[0] & 0x0F == 8:
                return
            size = head[1] & 0x7F
            mask = self.rfile.read(4)
            body = bytes(b ^ mask[i % 4] for i, b in enumerate(self.rfile.read(size)))
            if size == 14:
                print("fly roll %.1f pitch %.1f yaw %.1f thrust %d" % struct.unpack("<fffH", body), flush=True)

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        if url.path == "/fly-ws":
            return self.fly_socket()
        q = urllib.parse.parse_qs(url.query)
        if url.path in TYPES:
            name, kind = TYPES[url.path]
            with open(os.path.join(WEB, name), "rb") as f:
                return self.reply(200, f.read(), kind)
        if url.path == "/status":
            f = state["flying"]
            return self.reply(200, json.dumps({"name": "ARIA", "type": "drone", "mode": "flying" if f else "ground", "moving": f,
                                               "flying": f, "battery_v": round(3.9 + random.random() / 20, 2),
                                               "network": "RIFT", "rssi": -52}))
        if url.path == "/ping":
            return self.reply(200, "ARIA", "text/plain")
        if url.path == "/chirp":
            try:
                u = int(q.get("u", ["-1"])[0])
            except ValueError:
                u = -1
            if not 0 <= u <= 16:
                return self.reply(400, json.dumps({"ok": False, "error": "use /chirp?u=0-16"}))
            if state["flying"]:
                return self.reply(409, json.dumps({"ok": False, "error": "she's flying"}))
            print(f"chirps line {u}")
            return self.reply(200, json.dumps({"ok": True}))
        return self.reply(404, json.dumps({"error": f"ARIA has no {url.path}"}))


def register(host):
    body = b"name=ARIA&type=drone&capabilities=web:5014,talk:5014"
    while True:
        try:
            urllib.request.urlopen(f"{host}/register", body, timeout=2).read()
        except OSError as e:
            print(f"register: {e}")
        time.sleep(10)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--register", metavar="URL", help="register with this fleet list, as she does")
    ap.add_argument("--flying", action="store_true")
    a = ap.parse_args()
    state["flying"] = a.flying
    if a.register:
        threading.Thread(target=register, args=(a.register,), daemon=True).start()
    print(f"pretend ARIA on http://127.0.0.1:{PORT}/")
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
