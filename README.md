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
- Based on: [ESP-Drone](https://github.com/espressif/esp-drone) (Espressif, GPL-3.0; its flight core comes from Crazyflie)
- Firmware: `scripts/esp-drone`, ESP-Drone 9d5a4f8 (2026-10-26), unchanged so far. Builds with ESP-IDF v5.5 or v6.1.
- Licence: GPL-3.0, as ESP-Drone's
- Colours: cyan and white, like clouds
- Status: just started. Her board isn't built, and her firmware hasn't been compiled or flown yet.

### In the fleet

`CONFIG_ARIA_FLEET`, on by default (`components/drivers/general/wifi/aria_fleet.c`):

- **RIFT:** she keeps her own AP for the app and joins the highest fleet network she can see, then registers with whoever hosts the list. She only ever joins, she never hosts.
- **`:5014`:** `GET /status` (flying, battery, network, signal), `GET /ping`, `GET /chirp?u=<0-16>`.
- **ESP-NOW:** the fleet's "I'm here" beacon twice a second, saying "park" or "go". She sends it and doesn't listen, like MILA and NIKA.
- **Brainfuck:** the fleet's phrases, a tone per symbol on the buzzer deck and a flash of her blue LED, so she talks with or without the buzzer.
- **Safety:** the fleet can look at her and hear her, nothing in it can arm or fly her. It runs at the lowest priority, and with her motors spinning she won't scan, join a network or chirp, because a scan would cut the app flying her.
- **Her WiFi channel is 1**, the fleet's (ESP-Drone's default was 6).

Not done: the ladder's climbing while she's on a lower network (she looks again only when she has no network), and following a host's change mid-flight.
- Site: https://cursedprograms.github.io/ARIA-Robot-v00/

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
