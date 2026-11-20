# culdcept-nx: the session's report (2026-11-18 23:45 to 2026-11-20 12:00 AEST: the deadline extended by 24 hours)

Culdcept Saga (Xbox 360, title 424107DD, XDK 2.0.3424) ported with ReXGlue from renut_nx_ultimate_base 5017fe95. The
first day on the PC and in Ryujinx; from 2026-11-19 13:50 on the user's console as well (sys-ftpd, the user's own tests
and replays). Details and evidence: `docs/culdcept-notes.md`; the timeline: `docs/progress-log.md`; console steps:
`docs/console-testing.md`.

## What works

| Where | State |
|---|---|
| Console (the user's, handheld and docked) | Boot, movies (now decoded natively, smooth), title, Cepter creation with the Switch keyboard, save, the story, the board and its battles, Versus with up to four controllers (three of them virtual pads in the console test); the board at a mean of 58 fps with stutters (below) |
| PC (Renut on Vulkan) | The same, at 60 fps; every machine copy and the IDCT native checked on every call (see "Measured") |
| Ryujinx (Renut on deko3d, the Switch NRO) | The route to the first battle at about 60 fps; every verify line 0 differences |
| SD card folder | `D:\culdceptsaga\sdcard\switch\culdcept-nx` (rc6 plus the extracted disc, hard links to `D:\culdceptsaga\game`); not in git, not uploaded |
| The user's card | rc6's NRO (md5 aa861031..., read back after the upload) and configs/ put on `/switch/culdcept-nx` over FTP at 2026-11-20 08:10, the user's yes; culdcept.toml there was rc6's already; not yet started on the console |
| Package | `D:\culdceptsaga\builds\2026-11-20_0621_rc6` (commit 507786a3; rc5 of 04:19, rc4 of 2026-11-19 17:17 and earlier superseded): NRO (43.0 MB), culdcept.toml (shipped), configs/ (with `renut_profile_slow.toml`, the stutter profile's, and `culdcept_shipped.toml`), DKSH cache, ELF, source snapshot, README, PACKAGE_NOTES (what changed since rc4). The SD folder holds rc6 (06:21) |
| Web installer | `D:\culdceptsaga\culdcept-nx-installer` (the template's copy with Culdcept's game.json and release/); not published |

## Fixes made for this game (all in culdcept-nx)

1. Codegen: the script interpreter's two half-word jump tables, setjmp / longjmp, four vtable methods; the register diet
   everywhere except the run-time HLSL compiler (`non_volatile_as_local` broke its compile with X4801).
2. Renut at XDK 2.0.3424's addresses (291 hooks, Viva Pinata's device layout, the bool constants at 0x2700 / 0x2710,
   BeginTiling by hand); version 0x10 shader containers (handoff 01).
3. The models' alpha test at the console's 8-bit precision (handoff 03).
4. A movie skip hung the game in Ryujinx: Renut's GPU callbacks now also run at guest waits (handoff 05).
5. The board's 2D panels wrong with the fast draw: the device's SetFVF declaration counts as `kFieldDecl` (handoff 07).
6. SMAA off (the board and the HUD share one shader and one target).
7. The Cepter name on the console: a cancel completes with ERROR_CANCELLED (the keyboard came back after every cancel;
   handoff 09), and the name's system message box takes the focused button (the ImGui box never showed on the console,
   the game froze; handoff 10). Confirmed by the user on the console.
8. The dialogue box's faint vertical line: Renut's half-pixel move off for this game (its 2D quads were half a texel off
   their art; the text 5 % and the portraits 12 % sharper).
9. The battle's end fade: the game's own look (the user saw the same on an Xbox 360). Closed, unchanged.
10. Local multiplayer: an empty controller port now reads as zeros (the game read the previous pad's state, so the
    lobby took presses from empty ports; handoff 11), and pads that connect after the sign-in screen are signed in as
    guests (handoff 15). Four players joined Versus on the PC and on the console (three virtual pads).
11. The movies: the game's WMV9 video decoder replaced at its interface by FFmpeg's WMV3 (`culdcept_wmv_ffmpeg`),
    bit-identical to the game's in compare mode; the user: smooth on the console.
12. The board stutter (tonight; `docs/culdcept-notes.md`, the two stutter sections):
    - the profile read: ~230 frames of 40 ms or more in 4 minutes; in them 18 % file opens, ~22 % the game's JPEG
      decoder (the card art), 9 % the game's small-block free, 6.5 % a wait for its loader thread;
    - the game's data files kept open (`filesystem_keep_open`, handoffs 12 and 18): on the PC's board replay 595
      reopens in 300 s took a parked handle (no file-system request), in Ryujinx 713 in 700 s; an open that fails
      while handles are parked closes them and tries again (the console's limit on open files is not measured);
      on the console (prof3, the user's replay at last night's clocks, 2026-11-20 14:20) 877 reopens took a parked
      handle and the main thread's file-system requests fell 55 % (0 failed opens: the retry never ran);
    - machine copies of every function in the console profile that had none (102 -> 256), the decoder chain among them;
    - hand-written natives: the JPEG inverse DCT (`culdcept_hand_idct`, NEON lanes) and the Huffman decoder
      (`culdcept_hand_huffman`, the bit reader in registers); both bit-exact in unit tests (200,000 blocks; 44,924
      MCUs); on the PC the IDCT compared with the original on every call (604,800, 0 differences) and the Huffman
      decoder by its own check (600,000 MCUs, 0 differences); in Ryujinx (rc6's NRO, the route to the first battle,
      700 s) the Huffman decoder's own check 16,384 MCUs, 0 differences, and the hand's card art clean (the IDCT's
      NEON path); on the console (prof3) the IDCT a third cheaper, the Huffman decode 14 % cheaper;
    - not given a native, and why: the small-block free (its time is a list walk a native cannot shorten), the colour
      conversion (a native would make the same table lookups), the wait (time spent waiting for a file).
    - the file cache at 256 instead of 32 (prof4, the user's request; a toml line, not the shipped default): about 200
      files kept open, none closed for the cap, 0 failed opens, file-system time -26 %; the stutter numbers unchanged.
13. The game menu (2026-11-20, the user's request): no Framerate cap and no SMAA rows; the docked resolution's default
    1920x1080 in the menu, its help text and the code. Built for the PC and the Switch; not yet seen on screen.

## Handoffs for the base (`D:\culdceptsaga\out\handoff`, README.md there)

The user (2026-11-20 01:20): "please send anything universal back to the ultimate base including crash and rendering
fixes". 17 patches; 01-12 and 14-17 apply together in order on 5017fe95 and on the base's HEAD of 01:20 (607d9dbc); 13
is in the base already (port 191). Sent to the base's session (Ridge Racer 6's), which took all 16 at 2026-11-20 ~03:55
as **ports 207-222** (committed locally in the base, not pushed; its docs at 32581b7d), every key and default as set
here; 218 (the file cache) applied by hand beside sims3-nx's save fix (port 203); tested there on Katamari (PC and
Switch builds, the PC and Ryujinx into a level, 0 errors, keep-open reusing 39 / 45 handles). Handoff 18 (the file
cache's retry, 06:05) went in as 218's follow-up (3cf8012c). Nothing of the base was changed from here.

| Patch | What |
|---|---|
| 01_container_10 | Version 0x10 shader containers translated and accepted (rendering) |
| 02_renderdoc_game_frame | `banjo_renderdoc_game_s` gets its frame tick from the Swap |
| 03_alpha_test_unorm8 | Alpha test on the 8-bit grid (rendering; compile-time, off in the base) |
| 04_headless_keyboard_text | Test key: a headless keyboard prompt answers with given text |
| 05_callbacks_at_waits | Renut's GPU callbacks also run at guest waits / sleeps (a hang) |
| 06_thread_suspend_log | Diagnostic: every NtResumeThread / NtSuspendThread logged |
| 07_fast_draw_device_declaration | The device's own SetFVF declaration counts as `kFieldDecl` (rendering) |
| 08_fast_force_diag | Diagnostic: fast-draw field bits forced by time window |
| 09_keyboard_cancel_result | A keyboard cancel completes with ERROR_CANCELLED |
| 10_message_box_focused | Switch: a system message box answered with the focused button, logged |
| 11_input_clear_disconnected | XamInputGetState zeros an empty port's state |
| 12_keep_open | Read-only game files keep their host handle; no restat of read-only files |
| 13_built_ptr_scan_overwrites | The base's built-pointer scan: a pair ends at every write of its register (in the base) |
| 14_indirect_call_outside_image_log | An indirect call outside the image names its function and site (crash diagnostic) |
| 15_late_guest_signin | Pads that connect after the sign-in screen are signed in as guests |
| 16_input_script_more_pads | Test keys `input_script_p2` / `_p3` / `_p4` |
| 17_draw_log_targets | The draw log's window, its targets' addresses, clears and resolves |
| 18_keep_open_retry | 12's follow-up: a failed open gives up the parked handles and tries again |

The base's built-pointer scan (the crash Sonic 4 Episode I's port fixed) on Culdcept: 1 candidate, a false positive; 0
after the fix.

## Measured, and how

- Console (the user's sessions): the GPU 7.3 ms of 16.7 on the board; the frame-rate losses are game-thread hitches.
  The replay's profile (out/console_runs/prof1): main thread 35 % of a core (68 % peak), render thread 16 %, the process
  80 % of 400 %.
- Machine copies: 99 before tonight, checked on the PC with every copy guarded on every call (170 M calls, 0
  differences); the 256 of tonight: 245 of the 251 bulk copies reached on the board replay, 65.8 M calls compared, 0
  differences (verify4).
- Frame rates on the PC and in Ryujinx are proxies only; the console decides (load times and frame rates were not
  compared between emulator runs).

## What is next (the console steps: `docs/console-testing.md`)

1. Done 2026-11-20 (the user's replay with rc6 on the console): at last night's clocks (prof3, 14:17-14:26; CPU 1020,
   GPU 460.8, memory 1600 MHz) frames of 40 ms or more 313 -> 260 in the replay, time in them 24.1 -> 17.7 s, the
   longest 430 -> 409 ms, the board's 1 % lows 9.7 -> 12.1 fps. A first run (prof2, 13:56) at a memory clock of 2733
   MHz (an overclock profile active after the console's restart, set by neither session) gave the same stutter numbers.
   Left in the stutter frames: the JPEG chain (~23 %), the small-block free (~10 %), opens made by the path lookup (~7 %). Next levers: the path lookup without an
   open on a read-only device, a larger file-cache cap (`docs/culdcept-notes.md`, the last section).
2. PGO + LTO (not this session): `configs/renut_pgo.toml` is the training toml (docs/building.md).

## For your decision

- MSAA: off (msaa_level 1); `configs/renut_msaa2.toml` is 2x (changes the picture).
- The alpha-test fix changes the PC's picture to the console's; on in this port, off in its handoff.
- Renut's half-pixel move off for this game (the dialogue line); on in the base.
- System message boxes on the Switch are answered with the game's focused button and not shown (each is logged).
- The profiler stays on, its stack sampler off in the shipped toml (yours, 2026-11-19).
- The CPU clock stays as the console sets it (yours).
- The web installer's NRO carries the game's 7 translated shaders: publishing it is yours to decide. Nothing was pushed
  or published.
- No title update or DLC was needed.
