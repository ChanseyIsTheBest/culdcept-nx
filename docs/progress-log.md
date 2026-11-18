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
| (earlier) 00:20-01:00 | Renut's remap prepared: Culdcept's device layout measured equal to Viva Pinata's (XDK 2.0.3529); hook map 291 of 368 lines placed (Viva's lines, the pairs, signatures, Culdcept's own state tables); guest data addresses found; vivapinata-nx's edit and zone tools adapted; its two later XDK fixes (integer viewport, GetRenderState bits) scripted. |
