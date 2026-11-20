# culdcept-nx 1.0.0

Culdcept Saga (Xbox 360) on the Nintendo Switch, the first release. Install it with
[culdcept-nx-installer](https://github.com/ChanseyIsTheBest/culdcept-nx-installer) from your own copy of the disc: the
page checks it, downloads this program, and writes the game folder about 4.7 GB down to 2.6 GB. The repository's
[README](../README.md) has the steps, the controls and the settings.

## Since the release candidates (rc6)

- **Smaller game files, read as they were** (the installer's work, the port's reading):
  - the archives packed with LZ4 and the effects with ZXC, decompressed as the game reads them (3,273 -> 1,875 MB);
  - the movies re-encoded to H.265 and played on the Switch's video hardware, NVDEC (1,069 -> 414 MB; a WMV movie
    still plays);
  - the music and sound effects re-encoded to Opus, the music at 128 kbit/s (290 -> 171 MB; the voices stay XMA). The
    game sees its XMA files as they were, so loops and timing are its own.
- **Faster starts**: an index of the game folder (`cache/`) is kept and used at every start after the first. The packed
  files made it miss every time before (a 10.8 s scan in Ryujinx, 0.24 s from the index).
- **The game menu** (A + B + X + Y + L + R): no SMAA and no framerate-cap rows (neither suits this game), and 1920x1080
  is the docked default (in the menu and in the program).
- **The program**: version 1.0.0 with its own icon, built with link-time optimisation and PGO trained on a full
  replay of a game (the profile in `pgo-inplace/`).

## Measured

- **Console** (the user's replay, 2026-11-20): the Opus build played 25 sounds from Opus with no warning or error; the
  board holds about 60 fps on average (59-60 a 10 s window), with short drops when the game loads its data (1 % lows
  12-16 fps), as in rc6. The audio worker takes 1.96 % of a core (1.80 % with XMA).
- **H.265 movies on the console** (a benchmark of the credits, all 6,470 frames): 4.6 ms a frame on NVDEC, the worst
  9.3 ms, none late. In the game (2026-11-21): a 60 fps narration at 600 frames every 10 s, 2.6 ms a frame (the picture
  copied out of NVDEC by the VIC, the same bytes as FFmpeg's CPU copy, which took 18 ms), with its sound; the
  installer's movies keep each audio object whole, as the originals do (the first test files played slowly and
  without sound). AV1 was measured and not used (the browser's AV1 is larger than H.265 for the same picture, and
  the smaller SVT-AV1 file stutters on the console's CPU).
- **The installer** (Chrome, the full test): every movie in H.265, 634 sound files in Opus, a 2,576 MB zip in 545 s,
  every file checked against the game folder.

The details and every measurement are in [culdcept-notes.md](culdcept-notes.md).
