# culdcept-nx: the session's report (2026-11-18 23:45 to 2026-11-20 12:00 AEST: the deadline extended by 24 hours)

Culdcept Saga (Xbox 360, title 424107DD, XDK 2.0.3424) ported with ReXGlue from renut_nx_ultimate_base 5017fe95, on
the PC and in Ryujinx only (no console, nothing on its SD card). Details and evidence: `docs/culdcept-notes.md`; the
timeline: `docs/progress-log.md`; console steps: `docs/console-testing.md`.

## What works

| Where | State |
|---|---|
| PC (Renut on Vulkan) | Boot, intro movies, title, menus, Cepter creation, save, the story (movies, field scenes, dialogue), the board map and the first board battle (dice, hand of cards, Spell Selection) at 60 fps; the picture clean (alpha test fixed) |
| Ryujinx (Renut on deko3d, the Switch NRO) | The same route to the first battle at 60 fps (after two fixes: the movie skip's GPU callbacks, the fast draw's stale vertex layout for the 2D panels); every verify line 0 differences |
| Package | `D:\culdceptsaga\builds\2026-11-19_1031_rc2` (commit 3c62c206; rc1 of 07:43 is superseded: it had the fast draw off): NRO (39.7 MB), culdcept.toml (shipped), configs/ (renut.toml, renut_play.toml, renut_ab.toml, verify, PGO, profile, 720 docked, MSAA 2x, dynamic, textures_plain), DKSH cache (46 variants), ELF, source snapshot, README, PACKAGE_NOTES (console testing) |
| Web installer | `D:\culdceptsaga\culdcept-nx-installer`: a copy of renut_nx_ultimate_installer 2ce44cf with Culdcept's game.json, colours, icon and release/ (make_release.py: the NRO, the manifest of 5,860 game files); "shaders": false (the NRO carries the game's 7 shaders: 4 exist only at run time, 3 are version 0x10 containers the page's translator cannot read); not published |

## Fixes made for this game (all in culdcept-nx; base handoffs in `D:\culdceptsaga\out\handoff`)

1. Codegen: the script interpreter's two half-word jump tables (`[[switch_tables]]`), setjmp / longjmp, four vtable
   methods; the register diet on everywhere except the run-time HLSL compiler (`non_volatile_as_local` broke its
   compile with X4801; bisected in four PC builds).
2. Renut at XDK 2.0.3424's addresses (291 hooks, the device layout of Viva Pinata's XDK, the bool constants at
   0x2700 / 0x2710, BeginTiling by hand).
3. Version 0x10 shader containers translated (handoff 01).
4. The models' alpha test at the console's 8-bit precision (handoff 03, off by default for the base).
5. A movie skip hung the game in Ryujinx (3 of 3 runs at the 144 s press): Renut ran GPU callbacks only in its own D3D
   hooks; they now run when a game thread waits or sleeps (`banjo_renut_callbacks_at_waits`, handoff 05).
6. The board's 2D panels (dialogue box, battle HUD, a sign) drawn wrong in Ryujinx with Renut's fast draw: the game's
   SetFVF declaration is rewritten in place inside the device and the fast draw kept the old vertex layout (found
   with a field-forcing bisection: `kFieldDecl`). Fixed in the field map (handoff 07); the fast draw stays on.
7. SMAA off (no composite to anchor it: the board and the HUD share one shader and one target).

## Handoffs for the base (`D:\culdceptsaga\out\handoff`, README.md there; against 5017fe95, all eight apply together)

| Patch | What |
|---|---|
| 01_container_10 | Version 0x10 shader containers (XDK 2.0.3424's run-time compiler) translated and accepted |
| 02_renderdoc_game_frame | `banjo_renderdoc_game_s` gets its frame tick from the Swap (it captured nothing) |
| 03_alpha_test_unorm8 | Alpha test on the 8-bit grid (compile-time, 0 = off in the base) |
| 04_headless_keyboard_text | Test key: a headless keyboard prompt answers with given text |
| 05_callbacks_at_waits | Renut's GPU callbacks also run at guest waits / sleeps (the movie-skip deadlock) |
| 06_thread_suspend_log | Diagnostic: every NtResumeThread / NtSuspendThread logged |
| 07_fast_draw_device_declaration | The device's own SetFVF declaration counts as `kFieldDecl` (the fast draw's stale vertex layout) |
| 08_fast_force_diag | Diagnostic: field bits forced by time window, to bisect a fast-draw fault in one run |

Notes there too: the generator's half-table jump fault, the diet / D3DX compiler finding, vivapinata-nx's bool
constants, the installer's Katamari-only test, CG-13's census naming a copy's call line.

## Measured, and how

- CG-13 census twice (PC, the route to the battle): only VMX sites, the start-up self-check and a float-bits hash:
  BLUR_FPCR_ONE_MODE stays on.
- Calling-convention census: entry none; exit one r6 site in the HLSL compiler: EXIT_ABI off for this game.
- Keys from the log: no command-buffer replays, no inert draws: `ultimate_renut_replay_inherit_same = false`,
  `sasrt_gpu_skip_inert_draws = false`, `banjo_renut_tags_check = 256`.
- Audio census (PC, 65 windows): FL and FR only with the default speaker configuration and with
  `audio_speaker_config = 1`: the game mixes stereo; the default stays.
- Machine copies: 99 (Ryujinx and PC profiles), checked on the PC with every copy guarded on every call to the battle
  (170 M calls, 0 differences) and with poisoned dead registers (0 differences); in place on the Switch.
- Frame rates (proxies; the console decides; not compared between emulator runs): the PC holds 60 fps through the
  route; Ryujinx with the shipped settings about 60 fps on the board and in the battle (render thread about 7-10 ms
  a frame in emu19).

## What is next (the console steps: `docs/console-testing.md`)

1. Copy the package to the SD card with your game files; play the route with the shipped toml; keep `logs/`.
2. One run with `configs/renut_verify.toml` (0 differences expected).
3. The in-run A/B (`configs/renut_ab.toml`, 10+ minutes at a board turn).
4. A console profile (`configs/renut_profile.toml`) for the next round of machine copies and any hand-written native.
5. PGO + LTO (not this session): `configs/renut_pgo.toml` is the training toml; `tools/build_switch.bat` with
   `CMAKE_EXTRA=-DBANJO_PGO=generate` then `use` (docs/building.md).

## For your decision

- MSAA: off (msaa_level 1); `configs/renut_msaa2.toml` is 2x (changes the picture).
- `audio_switch_pump_pairs`: off as the base ships it.
- The alpha-test fix changes the PC's picture to the console's (holes gone); on in this port, off in its handoff.
- The web installer's NRO carries the game's 7 translated shaders (the page cannot make them): publishing it is yours
  to decide. Nothing was pushed or published.
- Decided (yours, 2026-11-19): the profiler stays on, its stack sampler (`switch_profile_stacks`) off in the shipped
  toml and every config but `configs/renut_profile.toml` (once in Ryujinx the app stopped inside the profiler's start
  with it on: 1 run of about 20).
- No title update or DLC was needed.
