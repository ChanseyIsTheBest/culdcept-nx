<p align=center><img src="tools/switch/icon/culdcept_icon.jpg" alt="Culdcept Saga" width="256" height="256"></p>

<h1 align=center>Culdcept Saga: Nintendo Switch port</h1>

<p align=center><b>culdcept-nx 1.0.1</b> · <a href="https://chanseyisthebest.github.io/culdcept-nx-installer/">web installer</a> · <a href="https://github.com/ChanseyIsTheBest/culdcept-nx/releases/tag/v1.0.1">release</a></p>

An unofficial native port of the Xbox 360 version of **Culdcept Saga** (OmiyaSoft / Jamsworks, Namco Bandai Games,
2008), made by static recompilation with [ReXGlue](https://github.com/rexglue/rexglue-sdk). No emulator. It is built on
[renut_nx_ultimate_base](docs/base-readme.md), which carries the fixes and optimisations of the other ports.

This repository contains no game files. You need your own copy of the game: the Xbox 360 disc image (`.iso`) or the
folder it was extracted to. No title update or DLC is needed.

## Playing it on the Switch

1. Open the web installer: **https://chanseyisthebest.github.io/culdcept-nx-installer/** (Chrome or Edge recommended;
   source: [culdcept-nx-installer](https://github.com/ChanseyIsTheBest/culdcept-nx-installer)).
2. Give it your disc image or folder, leave the two re-encode options ticked and press **Create culdcept-nx.zip**.
   Everything runs in your browser; nothing is uploaded.
3. Extract `culdcept-nx.zip` to the root of the SD card (it makes `switch/culdcept-nx/`).
4. In Sphaira, find the game, press **X**, choose **Install Forwarder**, set **Address Space** to **39-bit**, choose
   **Create Forwarder** and start the game from the Home screen.

- Coming from an older version: delete its `switch/culdcept-nx/game_root` folder first (an unpacked file left there is
  used instead of the packed one). A forwarder made from an earlier version keeps that version's icon: make it again.
- The movies are re-encoded to H.265 only in a browser with an H.265 encoder (Chrome or Edge on a recent graphics
  card); otherwise they stay WMV. A game folder copied straight from the disc also works, just larger.

## Features

- 1280x720 in handheld and 1920x1080 docked by default, at the console's stock clocks; 60 fps on average on the title,
  menus and board.
- Native renderer: reNut's Direct3D-level renderer on deko3d, the Switch GPU's own API.
- Smaller game folder (about 4.7 GB down to 2.6 GB): archives packed with LZ4 and ZXC, movies in H.265 played by the
  Switch's video hardware, music and sound effects in Opus.
- Two players on one console: every controller is its own player (handheld counts as one); single Joy-Cons are held
  sideways. **L + R + ZR** opens the controller screen.
- Face buttons by position, as on the Xbox 360 pad (B is the Xbox's A, A is B, Y is X, X is Y; + is Start, − is Back).
- Game menu (**A + B + X + Y + L + R** at once; + closes it): language, handheld (960x540-1280x720) and docked
  (1280x720-1920x1080) resolution, rumble, MSAA, four cores, button layout, sideways Joy-Cons and the menu's look.
  Saved in `culdcept.toml`, which also holds each player's button map (`[input_p1]`, `[input_p2]`).
- Four CPU cores instead of three (on by default, highly recommended): takes effect once `culdcept.toml` names your
  forwarder (`banjo_forwarder_program_id`).
- For a bug report, add `log_level = "info"` at the top of `switch/culdcept-nx/culdcept.toml`, play until the problem
  happens and send the `switch/culdcept-nx/logs/` folder.

## Known issues

- The board still has short frame-rate drops when the game loads its data.
- Online play (Xbox LIVE) is not supported.

## Building

Steps for the PC test build, the NRO, PGO and the tests are in [docs/building.md](docs/building.md). More:
[docs/culdcept-notes.md](docs/culdcept-notes.md) (the port step by step), [docs/console-testing.md](docs/console-testing.md),
[docs/optimising-functions.md](docs/optimising-functions.md), [docs/base-readme.md](docs/base-readme.md) and
[docs/ultimate-fixes.md](docs/ultimate-fixes.md) (the base and its fixes), [docs/progress-log.md](docs/progress-log.md).

## Credits and license

- **OmiyaSoft, Jamsworks and Namco Bandai Games**: creators of Culdcept Saga.
- **ChanseyIsTheBest**: this port.
- **[Tom Clay](https://github.com/rexglue/rexglue-sdk)**: the ReXGlue SDK, built on the work of the
  **[Xenia](https://xenia.jp)** team.
- **[StevensND](https://github.com/StevensND/nfsmw-nx)**: nfsmw-nx (the SDK's Switch layer, much of the method) and
  nfsmw-nx-installer, which this installer page is built on.
- **reNut and [masterspike52](https://github.com/masterspike52/reNut)**: reNut's Direct3D-level renderer (reused with its
  author's permission).
- **[plume](https://github.com/renderbag/plume)**, **[deko3d](https://github.com/devkitPro/deko3d)** and
  **[uam](https://github.com/devkitPro/uam)** by fincs.
- **[devkitPro](https://devkitpro.org) and [switchbrew](https://github.com/switchbrew/libnx)**: devkitA64, libnx and the
  portlibs (FFmpeg with averne's nvtegra backend, libopus).
- **[FFmpeg](https://ffmpeg.org)**, **[LZ4](https://github.com/lz4/lz4)**, **[ZXC](https://github.com/hellobertrand/zxc)**
  and **[Opus](https://opus-codec.org)**.
- The sibling ports (katamari-nx, rr6-nx, sasrt-nx, banjo-nx, blur-nx and the others in
  [docs/base-readme.md](docs/base-readme.md)), which share their fixes.
- The other libraries listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

An unofficial, fan-made port. No affiliation with OmiyaSoft, Bandai Namco, Microsoft or Nintendo. "Culdcept" is a
trademark of OmiyaSoft; the icon is the game's artwork. This repository contains no game files and no code generated
from the game; you provide your own legally obtained copy, and everything made from it (`app/generated/`, the
installer's files) is made on your own computer. Running homebrew requires custom firmware, which violates Nintendo's
ToS and can get a console banned.

GPL-3.0 (see [LICENSE](LICENSE)), like the ports it builds on; the SDK keeps its BSD-3-Clause license (see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)).
