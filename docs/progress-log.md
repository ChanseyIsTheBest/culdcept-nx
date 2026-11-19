# culdcept-nx progress log

Times AEST. The session runs from 2026-11-18 23:45 to 2026-11-19 12:00 (the request), PC and Ryujinx only.

| Time | What |
|---|---|
| 2026-11-18 23:46 | Start. Disc extracted (4.6 GB, 5,861 files, title 424107DD, XDK 2.0.3424 with D3D9LTCG). |
| 23:58 | `culdcept-nx` repo from the base's tracked files (5017fe95), committed. Ryujinx data folder `ryu-data\culdcept` (Katamari's system files, an empty SD card). |
| 00:05-00:12 | Codegen: first run clean; pointer sweep 80, gap sweep 54; the interpreter's two half tables fixed by `[[switch_tables]]`; setjmp / longjmp; the register diet audited (80 + 45 kept, 18 share_registers). |
| 00:13-00:35 | Katamari's game layer taken out (natives, hot lists, stage diag, tomls, icon, SD folder renamed). First PC build: the base's untracked inputs (DXC, FFmpeg's config, SDL shaders, host shader tools) copied in; the shader dump made empty (no container in default.xex). |
| 00:36 | First boot (Renut, hooks off): unregistered call 0x82197470, a run of vtable methods merged into one function; declared. |
| 00:48-00:51 | Boot on the emulated GPU: a null virtual call in the shader manager (sub_823814E0 -> the vertex shader's ID3DXConstantTable SetBool); the game compiles data\shader.fx at run time with D3DX. Diagnostic hook on D3DXCompileShaderFromFile added. |
| 01:04-01:19 | The D3DX compile fails with the register diet (X4801 "invalid parse tree"); bisection 1: diet off, compile and intro movies fine. Diet off for now. |
| 01:20-01:52 | Remap applied (291 hooks, offsets, edits, Viva's later fixes, the bool constants, the inherit tail, Viva's hand audit values). Version 0x10 shader containers converted (container_10.h). BeginTiling hooked by hand: the full title screen on Renut / Vulkan. |
| 01:56-02:35 | Menus, Cepter creation (headless_keyboard_text), the story. Models speckled: RenderDoc capture (banjo_renderdoc_game_s fixed), pixel history, alpha test log: GREATEREQUAL 1.0 vs interpolated white; alpha test at 8-bit precision fixes it. |
| 02:36-02:52 | Story route to the first board battle (gameplay on the PC). Calling-convention census: exit ABI off (one r6 site). Package tomls generated. Only 7 shaders in the game. |
| 02:53 | Switch shader package (7 shaders) and the first Switch build started; CG-13 census run along the route. |
| 03:10-03:30 | Switch NRO built (622 s, 39.9 MB). emu1 in Ryujinx (840 s, verify toml): the intro movie drawn, then no game frame after 25 s (not explained; Ryujinx's log shows no fault). |
| 03:33-03:36 | emu2 (150 s, the stall watch on): through the title, menus, Cepter creation, save and the story's intro at 60 fps, the verify lines all 0. The stop of emu1 did not come back. |
| 03:37-03:51 | emu3: the route to the first board battle in Ryujinx (840 s). Meanwhile: `tools/package_switch.ps1` and `dist/switch/README.txt` for Culdcept; the installer copy `D:\culdceptsaga\culdcept-nx-installer` (game.json, colours, icon; "shaders": false, the NRO carries its 7 shaders: 3 in default.xex as version 0x10, 4 made at run time). |
| 03:51 | emu3 stopped 3 s after the first frame (no dump: the stall watch's log is on the SD's switch/ folder). |
| 03:52-03:58 | emu4 with `tools/emu_watch.ps1`: stopped at 144 s (a deadlock: Ryujinx's CPU 0.9 -> 0.2 cores). The stall dumps: the main thread in the movie player's stop (`sub_8256CDC0`) waiting for its workers, three workers in their own NtSuspendThread: a ResumeThread that came before the worker's self-suspend was dropped on the Switch path (kept on Win32). |
| 04:02 | SDK fix `thread_keep_early_resume` (handoff 05). Register diet bisection 2 in the codegen config (non_volatile_as_local and non_argument_as_local on, elide_lr_stores off). PC build 23 started (codegen + SDK). |
| 04:12-04:35 | Diet bisection on the PC: round 2 (both register keys, elide off) X4801; round 3 (`non_volatile_as_local` alone) X4801; round 4 (both keys, the compiler's 3,457 functions excluded) compiles, the title at 60 fps. Kept. |
| 04:37-04:41 | Switch build 2 (diet round 4 + `thread_keep_early_resume`): 243 s, 41.2 MB. Committed e6480d07. |
| 04:41 | emu5: the route to the first battle in Ryujinx (840 s) with the fix. |
| 04:41-04:55 | emu5 (the fix): stopped at the same 144 s press. The early-resume guess was wrong. |
| 04:56-05:08 | SDK diagnostic `thread_suspend_log`; emu6/emu7 in Ryujinx and `susp1` on the PC: the same until the skip, then the XMV control thread waits for the renderer's frames-in-flight count, which only the frames' GPU callbacks count down, and Renut ran them only in its D3D hooks. |
| 05:10-05:20 | Fix: GPU callbacks also run when a game thread enters a wait or a sleep (`SetGuestWaitHook`, `banjo_renut_callbacks_at_waits`); the early-resume change taken out. Switch build 4 (76 s); emu8 passed the 144 s press. emu9: the full route (840 s). Handoffs 05 / 06 remade. |
| 05:18-05:32 | emu9: the full route to the battle, no stall (the callback fix holds). The user's screenshot: the board's dialogue box missing, a flickering image; emu9's captures: the map's 2D panels missing or red from 452 s on. |
| 05:32-05:43 | rdcemu1 (RenderDoc at 600 / 620 s + draw log): every panel right, in the capture and the screenshots. |
| 05:44-05:55 | runA on the PC: the route with the draw log, the audio census (FL/FR only, 65 windows) and the CPU profile. The draw logs match (238 draws, one shader pair): SMAA has no composite to anchor on, off. |
| 05:56-06:07 | emu10 (the texture memo's verify): every panel right. The panel fault: 1 of 3 runs, open (notes, "Missing 2D panels"; `configs/renut_textures_plain.toml` for the console). |
| 06:08 | Bulk copies: 56 from the emulator profile (emu9), 43 from the PC's (runA). Diet round 5: the 47 compiler-set functions hot in gameplay back in the diet (3,410 excluded). PC build 27. |
| 06:10-06:50 | PC build 27 (diet round 5, 99 copies): 2,373 s. |
| 06:45-06:57 | verify2 on the PC: the route to the battle with every copy guarded on every call (170 M calls, 0 differences), every verify 0, both shaders compiled; CG-13 again: the same VMX / self-check / hash sites, one-mode stays on. DKSH cache: 46 variants, 88 KB. Committed f67e880d. |
| 06:57-07:03 | poison1 (poisoned dead registers: 0 differences; `audio_speaker_config = 1`: FL/FR only, as the default). Switch build 5 (copies in place): 284 s, 41.6 MB. |
| 07:04-07:18 | emu11 (the copies build): 60 fps, no stall, verify 0, but the board's panels broken again (2 of 4). Installer release made (`make_release.py`), page checked in the browser pane. |
| 07:19-07:29 | rdcemu2 (RenderDoc at 470 / 560 s): Ryujinx ran slow with RenderDoc and only reached the story. |
| 07:35 | The panels' cause: the draw log turns Renut's fast draw off, and the two good runs had it on. `banjo_renut_fast_draw = false` built in and shipped. Switch build 6 (51 s); emu12 to confirm. |
| 07:31-07:42 | emu12 (the fast draw off, no draw log): every panel right (3 of 3 without the fast draw). Committed 5af434e3; package rc1 (`builds\2026-11-19_0743_rc1`); installer release refreshed. |
| 07:42-08:10 | Other sessions' Ryujinx runs and builds on the shared PC: waited. The fast draw costs the story's scenes in Ryujinx (render thread 15-16 ms): a fetch-word check written (06344559), Switch build 7. |
| 08:10-08:21 | emu14 (the fast draw on with the check): panels broken again, 0 catches: not the fetch words. Check taken out (da7810b6); the fast draw stays off. |
| 08:25-09:20 | The user: "try and fix the fast draw shortcut". Debug key `banjo_renut_diag_fast_force` (field bits forced by time window). emu16: the panels right with every field forced and with the "rest" group (1DFC0), wrong with none, textures, bools or the view packets. |
| 09:24 | The user: the deadline extended 24 hours (to 2026-11-20 12:00 AEST); and: compiling beside another session's build is fine. emu17: one field bit a window. Candidate: the device's own (SetFVF) declaration at 0x2E90 rewritten in place maps to kFieldTail, not kFieldDecl. |
| 09:19-09:34 | emu17 (one field bit a window): kFieldDecl alone brings the battle HUD back (kFieldShaders, which recomputes the declaration, once). The fix: the device's own SetFVF declaration bytes count as kFieldDecl (render/draw.h). Switch build 10. |
| 09:37-09:48 | emu18: not usable (started beside another session's compile; the boot at 0.5-17 fps, the timed route ended on the title). |
| 09:52-10:04 | emu19 (the fast draw on, the fix): every panel right, no stall, the render thread about 7-10 ms. The fast draw on again (built in, shipped; A/B 25 back). Committed 862b5706; Switch build 11. Handoffs 07 (the fix) and 08 (the bisection key); all eight apply together on 5017fe95. |
| 10:17-10:31 | emu20: the full route with the built-in defaults (no fast-draw key): every panel right, 100 % fast, no stall, verify 0. |
| 10:31 | Package rc2 (`builds\\2026-11-19_1031_rc2`, commit 3c62c206: the fast draw on); installer release refreshed. |
| 10:39-11:26 | The package's own toml in Ryujinx: emu21 stopped 4 s in, inside the CPU profiler's start (the stack sampler, likely); emu22 (plus headless), emu23 (plus SMAA on) and emu24 (unchanged) booted. Written down for the console. |
| 2026-11-19 afternoon | The user: the profiler on, the stack sampler off; an SD folder with the game files. `switch_profile_stacks = false` in the shipped toml (every config but renut_profile). emu25: boots, "Stack sampling: disabled", no stall, verify 0. Package rc3; `D:\culdceptsaga\sdcard\switch\` (the package's files, the game's 5,861 files as hard links). |
| (earlier) 00:20-01:00 | Renut's remap prepared: Culdcept's device layout measured equal to Viva Pinata's (XDK 2.0.3529); hook map 291 of 368 lines placed (Viva's lines, the pairs, signatures, Culdcept's own state tables); guest data addresses found; vivapinata-nx's edit and zone tools adapted; its two later XDK fixes (integer viewport, GetRenderState bits) scripted. |
