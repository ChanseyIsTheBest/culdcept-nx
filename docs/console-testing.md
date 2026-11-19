# culdcept-nx on the console: what to run, in what order

This session ran the PC and Ryujinx only (the request: no console, nothing on its SD card). These are the console
steps it leaves, written so that each one is a copy, a start and a log to keep. The package is in
`D:\culdceptsaga\builds\<date>_<name>\` (`tools/package_switch.ps1`); its `README.txt` has the SD layout.

## 0. Copy

1. `COPY_TO_SD_switch_culdcept-nx\*` to `sdmc:/switch/culdcept-nx/` (the NRO, `culdcept.toml`, `configs/`,
   `deko3d_shaders.bin`).
2. Your extracted disc into `sdmc:/switch/culdcept-nx/game_root/` (`default.xex`, `data/`, `eeprev/`, `layout/`;
   `$SystemUpdate` is not needed). No title update or DLC.
3. Start it from the Homebrew Menu (title override: hold R on a game, for the full memory; an applet launch has too
   little).

## 1. First start: the shipped settings (`culdcept.toml` as packaged)

Logs on, the stall watch at 1 s. Play: the logos, the title (START), New Game, the Cepter creation (the on-screen
keyboard asks for a name), the save, the story's intro, and the first board battle (the map, the hand of cards).
Keep `sdmc:/switch/culdcept-nx/logs/` afterwards. What to look for in `renut_001.log`:

- `[enhancements]`: the line of what is on.
- `[renut] frame pacing` every 10 s: the console's frame rate and 1 % lows (the numbers that count; the PC's and
  Ryujinx's are proxies only).
- `[stall]` / `[hitch]`: where every thread was during a stop of 1 s or more (Ryujinx stopped for good at a movie's
  start or skip until the GPU-callback fix, `banjo_renut_callbacks_at_waits`; if the console stops, these lines say
  where).
- `[renut] SMAA`: frames with SMAA (see the notes' "SMAA").
- `[audio]`: the stream's under-runs, if any.

Look at the board's 2D panels (the dialogue box under a portrait, the battle's Cepter / Creature Card panels,
"Spell Selection"): in Ryujinx they went missing or showed another texture (red rectangles) until the fast draw's
declaration fix in this build. If they go wrong on the console, first try `banjo_renut_fast_draw = false` in
`culdcept.toml` (the full draw path), then copy
`configs/renut_textures_plain.toml` over `culdcept.toml` and play the same part again; say whether they come back
(docs/culdcept-notes.md, "Missing 2D panels").

## 2. The picture check: `configs/renut_verify.toml`

Copy it over `culdcept.toml`, play the same route for 5 minutes. Every verify line must count 0 differences (the
log's `verify:` fields). Slow by design. Put the shipped toml back afterwards.

## 3. The in-run A/B: `configs/renut_ab.toml`

Copy it over `culdcept.toml`. Get to a board turn that waits for input (the battle's "Spell Selection"), put the
console down and leave it 10+ minutes. Each `[renut A/B]` line gives a target's on / off halves (30 s each); target 1
is the do-nothing control (its difference is the noise). Targets: 25 fast draw, 28 pipelines by content, 22 whole
buffers, 26 the setters' touch compare, 42 mesh objects.

## 4. A console profile (for the next round of machine copies, and the only source for hand-written natives)

Copy `configs/renut_profile.toml` over `culdcept.toml` (the CPU sampler and its stacks on). Play the board battle for
3-5 minutes (dice, moves, a creature battle), then quit from the Home menu. Keep `logs/` (`rex_profile.log` and the
`[profile]` lines). On the PC:

```bash
python tools/switch_hot_bulk.py <logs>/rex/rex_profile.log <package>/culdcept-nx.elf --write app/src/cpu/hot/localized_bulk.txt --merge
```

then rebuild (docs/optimising-functions.md, "Getting a console profile"). Hand-written natives are only made from
such a profile (the request).

## 5. Four cores (optional)

`banjo_forwarder_program_id = "<your forwarder's id>"` in `culdcept.toml`; the next launch of the forwarder has CPU
0-3 (README.txt, "Cores"). Compare the step 1 numbers with three and four cores.

## What to send back

`logs/` of each step (they hold no personal data beyond the console's own paths), the captures if
`nfsmw_captura_cada_s` was set, and what was seen (a stop, a wrong picture, the sound).
