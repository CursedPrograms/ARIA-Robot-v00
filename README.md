<p align="center"><img src="images/avatar_round.png" width="160" alt="ARIA"></p>

[![Twitter: @NorowaretaGemu](https://img.shields.io/badge/X-@NorowaretaGemu-blue.svg?style=flat)](https://x.com/NorowaretaGemu)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

<br>
<div align="center">
  <a href="https://ko-fi.com/cursedentertainment">
    <img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="ko-fi" style="width: 20%;"/>
  </a>
</div>
<div align="center">
  <img alt="Git" src="https://img.shields.io/badge/git%20-%23323330.svg?&style=for-the-badge&logo=git&logoColor=white"/>
</div>

# ARIA
## Aerial Roaming Intelligent Automaton

- Robot Type: Drone
- Hardware: Diymore ESP32 Drone DIY Kit E1, on order. Sold as an ESP32-S3 with 716 motors, but its parts list says ESP32-DEVKITV1, MPU6500 and 8520 motors, so her chip and pins get confirmed when she arrives. 3.7 V single-cell LiPo, not included.
- Based on: [ESP-Drone](https://github.com/espressif/esp-drone) (Espressif, GPL-3.0; its flight core comes from Crazyflie)
- Firmware: `scripts/esp-drone`, ESP-Drone 9d5a4f8 (2026-10-26), unchanged so far. Builds with ESP-IDF v5.5 or v6.1.
- Licence: GPL-3.0, as ESP-Drone's
- Colours: cyan and white, like clouds
- Status: just started. Her board isn't built, and her firmware hasn't been compiled or flown yet.

### Her pages

<div align="center">
  <img src="images/screenshots/controller-page.png" alt="ARIA's controller page, on a pretend ARIA" width="320"/>
  <img src="images/screenshots/fly-page.png" alt="Flying ARIA from a browser" width="520"/>
</div>

### In the fleet

`CONFIG_ARIA_FLEET`, on by default (`components/drivers/general/wifi/aria_fleet.c`):

- **RIFT:** she keeps her own AP for the app and joins the highest fleet network she can see, then registers with whoever hosts the list. She only ever joins, she never hosts.
- **`:5014`:** her controller page (`web/`, RIFT's Open button), `GET /status` (flying, battery, network, signal), `GET /ping`, `GET /chirp?u=<0-16>`.
- **ESP-NOW:** the fleet's "I'm here" beacon twice a second, saying "park" or "go", and she hears everyone else's: `/near`. That needs ESP-Drone's ESP-BOX joystick receiver off.
- **Little windows:** her app and page show battery, pitch, roll, signal and network, and each nearby robot with a rough distance. It's from radio strength, so walls and antennas change it: hence the ~.
- **Brainfuck:** the fleet's phrases, a tone per symbol on the buzzer deck and a flash of her blue LED, so she talks with or without the buzzer.
- **Flying from a browser:** on her own WiFi, `http://192.168.43.42:5014/fly` has two sticks over a WebSocket (`/fly-ws`). Her firmware refuses anyone on the fleet's network, caps tilt, yaw and thrust again, and her thrust lock and watchdog still hold.
- **Flying from her Android app:** `Android App/`, the same sticks and rules. Choose WIFI or BLUETOOTH in the app. Build it with `gradlew assembleDebug`; the APKs are kept in `Android App/apk/`.
- **Bluetooth (ESP32-S3 only: the S2 has no Bluetooth):** press PAIR in her first two minutes after power-up and she pairs like a remote, then that phone reconnects any time. Flying needs the encrypted link, so strangers can't. It shares the radio with her WiFi.
- **A gamepad or flight stick:** over Bluetooth, or plugged in with a USB-C adapter (the phone must support USB OTG). Gamepad: pair it with the phone. Right trigger is the throttle (let go and she powers down), left stick turns, right stick tilts, hold A to arm, B stops. If it disconnects, she disarms. Flight stick: the stick tilts, its twist turns, the lever is the throttle; hold the trigger to arm, button 2 stops. A lever stays where it's left, so bring it down before you let go.
- **The official apps:** [ESP-Drone Android](https://github.com/EspressifApps/ESP-Drone-Android) and [ESP-Drone iOS](https://github.com/EspressifApps/ESP-Drone-iOS) fly her over WiFi too. Their flight packets are the ones her own app sends. Their Bluetooth code is Crazyflie's and doesn't reach her.
- **Safety:** the fleet can look at her and hear her, nothing in it can arm or fly her. It runs at the lowest priority, and with her motors spinning she won't scan, join a network or chirp, because a scan would cut the app flying her.
- **Her WiFi channel is 1**, the fleet's (ESP-Drone's default was 6).

`python scripts/fake_aria.py` is a pretend ARIA on this PC, for trying the page and RIFT's link with no drone.

Not done: the ladder's climbing while she's on a lower network (she looks again only when she has no network), and following a host's change mid-flight.
- Site: https://cursedprograms.github.io/ARIA-Robot-v00/

<div align="center">
  <img src="images/aria_avatar.jpg" alt="ARIA avatar: a human representation of the drone" width="320"/>
</div>

ARIA is the flying member of the DREAM Robotics fleet. The plan is the same as the rest of the fleet: she joins RIFT's network, answers `/status`, and talks to the others. ESP-Drone already gives her Wi-Fi control from an app or gamepad, stabilize mode, and height-hold and position-hold with extension boards.

Credit: [Farica☆Kimora](https://github.com/Farica-Kimora) © Cursed Entertainment

---

<br>
<div align="center">
&copy; Cursed Entertainment 2026
</div>
<br>
<div align="center">
<a href="https://cursed-entertainment.itch.io/" target="_blank">
    <img src="https://github.com/CursedPrograms/cursedentertainment/raw/main/images/logos/logo-wide-grey.png"
        alt="CursedEntertainment Logo" style="width:250px;">
</a>
</div>

---

<!-- DREAM-ECOSYSTEM:START -->
## The DREAM Robotics ecosystem

**Minds** — [DREAM](https://github.com/CursedPrograms/DREAM) companion · [TINA](https://github.com/CursedPrograms/TINA) engineer · [NINA](https://github.com/CursedPrograms/NINA) caretaker · [RIFT](https://github.com/CursedPrograms/RIFT) fleet hub · [LYCEA](https://github.com/CursedPrograms/LYCEA) training school  
**Robots** — [NORA](https://github.com/CursedPrograms/NORA-Robot-v00) · [MILA](https://github.com/CursedPrograms/MILA-Robot-v00) · [WHIP](https://github.com/CursedPrograms/WHIP-Robot-v00) · [KIDA-00](https://github.com/CursedPrograms/KIDA-Robot-v00) · [KIDA-01](https://github.com/CursedPrograms/KIDA-Robot-v01) · [IDA](https://github.com/CursedPrograms/IDA-Robot-v00) · [ARM](https://github.com/CursedPrograms/ARM-Robot-v01) · [NIKA](https://github.com/CursedPrograms/NIKA-Robot-v00) · [ARIA](https://github.com/CursedPrograms/ARIA-Robot-v00)  
**Remotes** — [RITA](https://github.com/CursedPrograms/RITA) handheld remote  
<!-- DREAM-ECOSYSTEM:END -->
