# Culdcept Saga port: notes

This file records the port: what each step found, with its evidence. The project starts from a copy of the ultimate base
(`D:\renut-ultimate\base`, renut_nx_ultimate_base at 5017fe95: its files, not its history), read-only. The method docs are
renut-nx's (`D:\Banjo kazooie port\banjo-nx\docs`). The nearest port by XDK is vivapinata-nx (Viva Pinata, XDK 2.0.3529,
`D:\viva pinata\vivapinata-nx`, read-only): its hook remap and device layout tools are the model for Renut's here.

The request (2026-11-18, 23:45 AEST): port Culdcept Saga to the PC and the Switch with ReXGlue from the ultimate base, the
Switch the main goal; work uninterrupted until 12:00 AEST on 2026-11-19; the PC and Ryujinx only (no console, nothing on
its SD card); a package for the console by then.

## Decisions (made without asking, as the request said)

| What | Decision | Why |
|---|---|---|
| Game files | `D:\culdceptsaga\game` (`tools/extract_iso.py`), outside the repo | the request |
| `app/assets` | a junction to `D:\culdceptsaga\game` | no second 4.6 GB copy on a D: drive with ~20 GB free |
| Internal project name | `renut` (as the base) | its CMake, post-codegen passes and renderer glue name `renut_*`; the NRO, its title and the SD folder are culdcept-nx |
| Untracked build inputs | copied from the base's working tree: DXC (`app/thirdparty/reblue-XenosRecomp/thirdparty/dxc-bin`), FFmpeg's generated config (`config.h`, `avconfig.h`, `ffversion.h`, the `*_list.c`), SDL3's GPU shaders (`.hlsl`), the host shader tools (`out/glslang`, `out/deko3d_tools`) | git-ignored in the base, so not in a copy of its tracked files; the build stops without them |
| Bootstrap generator | the base's built `sdk/out/win-amd64/rexglue.exe` (+ `rexruntime.dll`) in `out/bootstrap` for the first codegen runs | the PC build builds its own from `sdk/` and regenerates with it |
| First boot | Renut compiled, its hooks not registered (`CULDCEPT_RENUT_HOOKS=OFF`), `banjo_renderer = "emulated"` with `gpu_plugin = "xenos"` | separates CPU / kernel problems from renderer problems (peggle2-nx's order of work) |
| Icon | a plain placeholder (`tools/switch/icon/culdcept_icon.jpg`: the name on a gradient, no game art) | the user can replace it with their own |
| Hand-written natives | none (the header keeps only the base's common code) | the request: only from a console profile |

## The game (2026-11-18)

- ISO: `Culdcept Saga [NTSCU].iso`, XGD2, 7.8 GB image, 5,861 files, 4.6 GB extracted.
- `default.xex` (3,207,168 bytes, `CuldSagaUS.Pe`): title **424107DD**, media `1BAD2474`, version 0.0.0.1, built 2007-08-23;
  base `0x82000000`, entry `0x82356D28`, image `0x01F90000` (33 MB); AES + LZX. Code range (generated `REX_CODE_BASE /
  SIZE`) `0x82170000-0x82678DB8` (5.3 MB).
- XDK: every static library **2.0.3424** (XAPILIB, XBOXKRNL, XNET, XONLINE, XHV, LIBCPMT, D3DX9, XGRAPHC, XMEDIA, X3DAUD,
  **D3D9LTCG**, **XAUDLTCG**); imports xboxkrnl.exe (250) and xam.xex (132) at 2.0.5766. Between Feeding Frenzy's 2571 and
  Viva Pinata's 3529 in renut-nx's `docs/xdk-versions.md`, and like Viva the link-time-code-generation Direct3D library.
- Data: `data/DATA00-06.RSP` archives (2.5 GB, packed: header `00C80000`), `data/EFFECT` (1,901 files), `data/MOVIE` (WMV),
  `data/SOUND` (XMA, XWB), `eeprev` (923 RSA / RSM / RST / RSE files, the map events), `layout`, `$SystemUpdate`. The
  engine is "RSLib" (`data/shader.fx`, an HLSL effect source). No other code module.
- **No shader container in default.xex** (`tools/renut_shader_dump.py`: 0 vertex, 0 pixel) and none in the unpacked data
  (16 stray magic matches in 5 GB). The shaders come from packed data or from HLSL compiled at run time; Renut's PC capture
  is the route (see "Shaders").
- Title update and DLC: none in `D:\culdceptsaga`.

## Code generation (2026-11-19)

- Config: `app/renut_config.toml`, the base's codegen keys that need no per-game audit. First run: 6 s, 169 files, no
  error (the base's generator).
- **Pointer sweep** (`tools/pointer_fixpoint.py`): 7,188 gaps, 89 candidates, 80 accepted; one removed by hand
  (`0x82203428`, "built at" exactly 0x14 after its `lis/addi`: a jump table's base, katamari-nx's warning). **Gap sweep**
  (`tools/gap_sweep.py`): 324 candidates, 54 accepted. `tools/override_labels_check.py`: 0 suspicious of 133.
- **Jump tables** (`tools/switch_check.py`): 761 `bctr`; one table wrong: `sub_821EDAA0`, the script interpreter's opcode
  dispatch (152 opcodes, a halfword table at `0x82002CE8` read with `rlwinm r0,r11,1 / lhzx / rlwinm r0,r0,2`, offsets in
  words from `0x821EDB88`). The generator made case 2k -> the base and case 2k+1 -> entry k, so every opcode would have run
  the wrong handler, and the handlers reached only through the table were not discovered at all (15 table jumps inside
  them had no generated code). Fixed with a `[[switch_tables]]` entry (`app/overrides.toml`); that exposed one more table
  of the same shape at `0x821FDBD8` (9 cases), fixed the same way. After both: 408 switches, 0 wrong, 0 missed, 0 table
  jumps without code. (A generator fault with this table shape: a handoff for the base, `out/` of the work folder.)
- **setjmp / longjmp** by shape (Katamari's shapes): setjmp `0x82652470` (the hook word check, then f14-f31 and r13-r31 into
  the buffer at r3), 7 callers; longjmp `0x82652230` (`mr r7,r3` ... `lfd f14,0(r7)`), 12+ callers.
- `_CxxThrowException` is `sub_82391F50` (copies the 32-byte record from `0x820FB808`, stores its two arguments, calls
  RaiseException `sub_8239F108`), given as `--noreturn` to the audits.

### Codegen options: the register diet (2026-11-19)

All on. `tools/non_argument_audit.py app/generated --noreturn=82391F50` on code generated WITHOUT the options: ENTRY 298, RET
36 lines; `tools/diet_keep_list.py` writes the keep list (80 ENTRY and 45 RET functions; the save / restore helpers are
kept by name). `non_volatile_as_local_keep = ["v64-v127"]` from the start (katamari-nx's rc3 finding).
`tools/read_before_write.py` on code generated WITH them: 26 functions; 8 are setjmp and its seven callers (the env saves
into the jump buffer, which the codegen handles), the other 18 take `share_registers` (`app/config/renut_funcs.toml`).
After that only the setjmp ones remain. `non_argument_audit.py --with-option`: one HANDOVER, setjmp's caller
`sub_823CC2C0` -> `sub_82457DF8` saving scratch r11 into the jump buffer: value-independent.

### The register diet breaks the game's HLSL compiler: on everywhere but the compiler (2026-11-19)

The game compiles its shaders at run time: the shader manager (`sub_82380F70` / `sub_82381030`, the object at
`0x83F3BAF0`) calls D3DXCompileShaderFromFile (`sub_823A0488`) on `GAME:\data\shader.fx` for `VS00` / `PS00` (vs_3_0 /
ps_3_0), with the XDK's whole D3DX HLSL compiler linked statically (2,589 functions reachable from it).

- With the register diet on, every boot stopped 1 s after the audio started: "Call to invalid or unregistered function
  at 0x00000000" from `sub_823814E0`, a call through slot 13 (`SetBool`) of the vertex shader's ID3DXConstantTable,
  which was null. The diagnostic hook (`culdcept_diag_shader_compile`, `app/src/culdcept_diag.cpp`) shows the compile
  failing: `= 80004005`, and with an error buffer of its own: "error X4801: Microcode Compiler internal error: invalid
  parse tree". The game's error path carries on with no shader and no constant table.
- `banjo_cpu_verify` (every post-codegen form checking itself): no difference before the stop.
- **Bisection 1: the diet off** (`non_volatile_as_local`, `non_argument_as_local`, `elide_lr_stores` false; everything
  else the same, post-codegen in full): both compiles return 0 with their shaders and constant tables, and the game goes
  on to its intro movies (`data/MOVIE/CI_00-02.wmv`) and the title music. So one of the three keys mistranslates
  something in the compiler; the audits (`non_argument_audit.py`, `read_before_write.py`) did not see it.
- **Bisection 2** (`non_volatile_as_local` and `non_argument_as_local` on, `elide_lr_stores` off; PC build 23,
  `out/pc_runs/diet2`): X4801 again. So `elide_lr_stores` is not (alone) the cause. The setjmp sites are not it either:
  the generator copies a setjmp caller's C++-local registers into `env` and back after a longjmp
  (`sdk/src/codegen/function_graph.cpp`). Only two C++ exceptions are thrown in the whole game, none in the compiler.
- **Bisection 3**: `non_volatile_as_local` alone (build 24, `out/pc_runs/diet3`): X4801. So that key mistranslates
  something the compiler runs.
- **Round 4**: both register keys on everywhere except the compiler (it runs at boot only): the functions reached by
  direct calls from `sub_823A0488` (2,589) and every function of its dense blocks `0x823A0000-0x823F0000`,
  `0x82420000-0x82440000`, `0x82470000-0x82560000` (its virtual methods, which direct calls do not reach): 3,457
  functions (`out/d3dx_compiler_set.txt`), in 121 `non_volatile_as_local_exclude` ranges and in
  `non_argument_as_local_keep` (`tools/diet_exclude_ranges.py`). Build 25.
- (Superseded by rounds 4 and 5:) the diet stayed off until the key and the function were found (the next bisection: each key alone, then the compiler's
  functions kept out of it). It is a CPU cost on the console (katamari-nx: the save / restore helpers were ~20 % of
  its main thread's ARM samples without it), not a picture one. `out/renut_config_diet.toml` keeps the diet's config.

### Calling convention (2026-11-19, tools/cc_entry_census.py and tools/cc_exit_census.py)

- Entry: "pointer-reached functions reading v0 or v32-v63 on entry (must be none): none". `SASRT_REG_LIVENESS_HOOKS_ABI`
  stays on.
- Exit: 5,435 indirect call sites, 15 with an EXIT_DEAD register read after the call. 14 are r0, each in
  `twi 31,r0,22` (TO=31: an unconditional trap, MSVC's bounds-check break; the value of r0 does not matter) in
  `sub_823C8608`, `sub_82295F08`, `sub_822962D8`, `sub_82183AE0`, `sub_823EBBB0`, `sub_822CF098`. One is r6 in the HLSL
  compiler's `sub_823E2C28` (node 214): a loop's bitmask (`and. r11,r10,r6`, `or r10,r10,r6`) reached after its
  indirect handler call; not shown value-independent. So **`SASRT_REG_LIVENESS_EXIT_ABI` is off for this game**
  (`tools/reg_liveness.py`'s default here: everything live at a pointer-reached function's exit), as the method says.

### Functions found at run time

- First PC boot: "Call to invalid or unregistered function" at `0x82197470`. A run of vtable methods (the vtable at
  `0x82157180` and its neighbours) the scanner merged into one function from `0x82197460`: two this-adjusting thunks and
  two leaves, declared with their ends (`app/config/renut_funcs.toml`). `tools/wide_pointer_scan.py --all`: 805 hits, of
  which the data-referenced starts inside bodies are SEH scope tables (`0x82158xxx`), `.pdata` (`0x82164xxx`) and Shift-JIS
  text; the "built at" ones are switch cases. Only the four vtable methods were real.

## Shaders (2026-11-19)

- **The whole game uses seven shaders** (3 vertex, 4 pixel; `out/renut_shader_dump`, from Renut's captures on the PC):
  two compiled at run time from `data\shader.fx` (VS00 / PS00, vs_3_0 / ps_3_0), five from its data. All are made in the
  first second of the boot, so the package is complete from any run. Their constants are few: PS00 reads `$flgTexOn`
  (bool), `Samp`, `litSpe`.
- **Version 0x10 containers** (flags `0x102A1000` / `0x102A1001`): XDK 2.0.3424's compiler ("Microsoft (R) Xbox 360
  Shader Compiler 2.0.3424.0") writes a container between Feeding Frenzy's 0x0E and the 0x11 Renut reads. Renut refused
  them ("not a shader container", 16 then 304 draws not drawn). Measured on VS00 / PS00: the 0x24-byte header, the
  constant table and the shader info (physical offset, size, field8 ... the interpolators, the vertex elements) are 0x11's;
  only the definition table differs (two more header words before its size and one after it: the definitions start at
  +0x20, not +0x14). `app/thirdparty/reblue-XenosRecomp/XenosRecomp/container_10.h` rewrites that table in 0x11's form for
  the translator (keyed by the original's hash, as container_0e.h), and Renut accepts 0x10 at its four checks
  (`d3d/hooks/shader.cpp`, `render/fetch_filters.cpp`). A base handoff (any game at this XDK that compiles at run time).
- The Switch package (`tools/switch_shaders.ps1`): 7 shaders, 16 KB of GLSL (`app/out/renut_switch/deko3d_glsl.bin`).

## The picture on the PC (Renut on Vulkan, 2026-11-19)

- **Only the left half of the title** at first: BeginTiling was not hooked (Viva's 82462B58 scored 0.68 by signature,
  under the map's 0.70) while EndTiling was. `sub_8218CD20` stores the tile count (`stw r5,12472(r31)`), the game calls it
  once (0x82380804): hooked by hand (`tools/renut_hook_map_culdcept.txt`). The whole title since.
- **Every 3D model and the ground speckled with holes** (the story's field scenes): found in a RenderDoc capture of the
  PC build (`banjo_renderdoc_game_s`, which needed its frame tick moved to the Swap: it was only called from N&B's
  appMainDraw hook, so the key captured nothing in the base). `rdc_history.py` on the speckled pixels: the pixel shader
  "discarded" them; the translator reports no shader with a kill instruction, so it is Renut's alpha test. The textures
  are bound with alpha forced to 1 (swizzle BGRO; the ground's saved texture alpha is 255 everywhere), so the tested alpha
  is the vertex colour's. A log of the alpha tests the game sets: ALPHAFUNC GREATEREQUAL with ALPHAREF 255 (threshold
  1.0), also 1/255 and 0. A white vertex colour interpolated perspective-correctly comes out as 0.99999994 at scattered
  pixels, which fail `>= 1.0`. Fixed in `shader_common.h` (`RENUT_ALPHA_TEST_UNORM8`, on here): the alpha test compares
  alpha and reference rounded to the 8-bit grid of the game's colour targets (an ALPHAREF is k / 255). The field scene
  is clean since (`out/pc_runs/at3`). Renut's early alpha off (`banjo_gpu_early_alpha=false`) had changed nothing. A base
  handoff, off by default there (it changes a picture: the rule of off-by-default.md section 1).
- **Gameplay reached**: title, main menu, New Game, the Cepter creation (the name from `headless_keyboard_text`, a test
  key added to the SDK for this), the save, the story's intro, its field scenes and the first board battle (the map, the
  hand of cards, "Spell Selection"), about 540 s from boot with the scripted route (`out/pc_runs/story2`).

## CG-13's census (2026-11-19, `tools/cg13_census.ps1 -Seconds 900`, the PC build, `out/cg13_1.txt`)

The route from boot through the story into the first board battle (89 windows of 10 s; the battle from 557 s to 889 s,
its spell phase). About 5,300 flush-mode switches a second in the game code (each an `msr fpcr` on the console that the
one-mode build leaves out). Windows with a denormal: 33 sites, cumulative:

| Share | Where | What it is | Verdict |
|---|---|---|---|
| 67.8 % | `sub_8228BF08` `vmsum4fp128` | VMX | the Xenon's VMX flushes denormals already: the same result |
| 7.7 % | `sub_823820B0` `vmulfp128` | VMX | as above |
| 20.1 % | `sub_8245B5D0` `lfs f0,268(r1)` / `stfs f0,156(r1)` (and its `vsubfp`) | a float copied from one stack slot into a 16-byte block at r1+144..159 that `lvx128 v13` then loads for VMX arithmetic | the value only reaches VMX code, which treats a denormal input as zero: the same result |
| 4.3 % | `banjo::FormsSelfCheck` | the app's own start-up check of the instruction forms | not game code |
| < 0.1 % (1-2 events) | `sub_825183E8` (`stfs f1-f4` then the four words added as integers), `sub_824AD460` (`lfs` passing them) | four floats hashed by their bits into a key | a denormal component would hash as 0, the same way every time in a run: harmless |

None divides or normalises such a value, guards against zero or feeds a replay. **BLUR_FPCR_ONE_MODE stays ON** in the
Switch build. To run again after a large codegen change (the register diet, if it comes back).

## Keys from the log (2026-11-19, blur-nx's rule, port 177; the same run, 89 windows)

- `sasrt_gpu_skip_inert_draws`: 0 in every window: **false**.
- 0 draws replayed from recorded command buffers (about 14,300 immediate draws a second; the game links no recorder):
  `ultimate_renut_replay_inherit_same` has nothing to inherit (N = 0): **false**; `banjo_renut_tags_check` sees no
  recording, so no chunk changes without a mark (M = 0): **256**.
- Set in `app/culdcept_switch_defaults.toml` (built in) and the package tomls (`tools/make_culdcept_tomls.py`).

## Renut at this game's addresses (2026-11-19)

- **The device layout is Viva Pinata's.** `tools/d3d_pair_culdcept.py` pairs Viva's functions with Culdcept's (unique
  shapes, the two state tables, aligned call sites): 3,079 pairs. `tools/renut_offset_pairs_culdcept.py` over the 414
  aligned Direct3D pairs: 1,556 of 1,658 device accesses at the same displacement; each of the others is a global constant
  (`lis` + displacement), not a device field. `Direct3D_CreateDevice` (`sub_82173000`) allocates 0x5080 and clears 0x5000,
  as Viva's. So vivapinata-nx's Katamari -> XDK 2.0.3529 zones apply as they are.
- **State tables** (`tools/renut_state_tables.py`): render `0x828B7500` x97, sampler `0x828B7990` x20, every row paired by
  its default (Viva's 97 + 20).
- **Guest data** (`tools/d3d_data_addrs_culdcept.py`): format table `0x8214F720` (Viva's 128 bytes word for word); build
  qword `0x828B7D40` (2.0.3424.0, the device init `sub_82182DE0`); VdGlobalDevice record `0x820007B0` (D3D_LockResource
  `sub_82172120`); HiZ owner `0x83F40478` (CreateSurface `sub_82171AE0`, Destroy `sub_82171F20`); no-large-pages flag
  `0x83E323BC` (XPhysicalAlloc `sub_823582D0`); XMemFree `sub_82357C60` (the device Release `sub_821730E8`: `lis r4,0x2480;
  lwz r3,-4(r31); bl`); GetCurrentThreadId `sub_8239EA68`; no D3DCAPS9 (no GetDeviceCaps).
- **Hooks** (`tools/renut_hook_map_culdcept.py`): the base's 368 hook lines through Viva's lines for the same Katamari /
  N&B address to Viva's address, then the pairs: 289 placed; the signature matcher (`tools/renut_sigmatch.py`'s score,
  >= 0.70 and 0.05 ahead) placed 6 more Viva hooks the pairing missed (LockSurface 0.98, EndTiling 0.88, DrawIndexedVertices
  0.85, Lock2DSurface 0.86, SetPixelShaderConstantB 0.79, SetCommandBufferPredication 0.89). The 184 render / sampler
  state hooks come straight from Culdcept's own tables by row (rows 97-100, HiZWriteEnable / LastPixel / LineWidth /
  Buffer2Frames, are not in this XDK). Hand answers (`tools/renut_hook_map_culdcept.txt`): IsFencePending is inlined
  here (`sub_82183368` at `0x821833E4`; the pairing's answer was KickOff); GetRenderState's Wrap10-15 rows all point
  at `0x82400F98`, `li r3,0; blr`, a body the linker folded with game functions: not hooked. Result
  (`tools/renut_hook_remap_culdcept.py`): **291 hooks placed, 77 unhooked** (functions this XDK lacks or inlines:
  GetDeviceCaps, BlockOnFence, the command-buffer recorder, memexport, tessellation, the volume / array textures,
  D3DVertexShader_Bind, XGRegisterPixelShader ...), two lines sharing one function (the first keeps it).
- **Device offsets**: `tools/renut_device_offsets_culdcept.py` (vivapinata-nx's zones, as the layout is the same):
  150 annotated lines; `tools/renut_port_culdcept.py` (vivapinata-nx's edits with this game's data addresses): the
  device front (no `m_Predicated_PendingMask2`, the ring at +0x28, 97 render states, the constants at 0x400), the
  struct and its asserts, the field map, the chain, the 24-byte last chunk, no 41 -1 words, the automatic front buffer
  (its own InitializePresentationParameters `sub_821828F0`), and 97 unannotated literals by the zones.
- **vivapinata-nx's later fixes for this XDK** (`tools/renut_xdk3424_fixes.py`): the device's viewport holds integers
  (x, y, width, height; the z range floats), Katamari's seventh word is this XDK's ScissorRect; GetRenderState's
  `bits(...)` fields and the wrap setter's words -128.
- **Found here, not in Viva's tree**: the bool constants. The base writes them at Katamari's 0x2780 / 0x2790; this
  layout's are 0x2700 / 0x2710 (Culdcept's SetPixelShaderConstantB `sub_8217C790` indexes word 2500 + start / 32).
  vivapinata-nx still has 0x2780 / 0x2790 (a latent fault there: for a handoff note). Also command_buffers.cpp's inherit
  tail (its assert and the diagnostic loop: 0x2700).

## The Switch build in Ryujinx (2026-11-19, `tools/run_ryujinx.ps1`, `out/emu_runs`)

- The NRO (`app/out/switch/renut.nro`, 39.9 MB; SASRT_HOT_IN_PLACE ON, no copies yet) with the verify keys
  (`out/emu_tomls/emu_verify.toml`: `configs/renut_verify.toml` plus the route's `input_script`, a capture every 15 s,
  `headless`, `headless_keyboard_text = "Cepter"`).
- **emu1** (840 s, the stall watch off): the intro movie drawn (capture 15 s: the OmiyaSoft / Jamsworks logos), frames at
  29 fps until 25 s, then no game frame, no 10 s report and no file read for the rest of the run; the captures say "no
  game image". Ryujinx's own log shows nothing at that moment (no guest fault, no service error). One "fast draw
  verify" line: the kept pipeline's KEY was the same as the one made; only the pipeline object differed (a provisional
  pipeline replaced; not a state difference).
- **emu2** (150 s, `banjo_stall_watch_seconds = 5`, `banjo_stall_watch_waits = true`: `out/emu_tomls/emu_stall.toml`):
  the same route went through: the logos, the title, the main menu, the Cepter creation, the save, the story's intro
  (captures 15-120 s), 60 fps from the title on (the 10 s frame pacing lines), every verify line 0 differences, no
  warning. The emu1 stop did not come back; the stall watch stays on in every emulator toml, so that a second one is
  caught with every thread's place (the log's "[stall]" lines).
- The emulator's save goes to `sdcard/switch/culdcept-nx/renut/B13EBABEBABEBABE/424107DD`: moved out into the run's
  folder before a route run (an existing save changes the route), as `out/pc_user` on the PC.
- **emu3** (840 s, the route): stopped 3 s after the first frame (the first movie starting); no stall dump, as the
  watch's log went to `sdmc:/switch/banjo_profiler.log` and an empty dump was looked for in the port's folder.
  **emu4** (300 s, with `tools/emu_watch.ps1` beside it: Ryujinx's CPU use every 5 s, an on-demand dump when the game
  image stops): stopped at 144 s, in the story, at an A press; Ryujinx's CPU fell from ~0.9 to ~0.2 cores (a
  deadlock, not a spin). The stall dumps (`out/emu_runs/emu4/banjo_profiler.log`, named with
  `tools/switch-cpu-profile.py --elf app/out/switch/renut`):
  - the main thread in KeWaitForSingleObject from `sub_8256CDC0` (called from `sub_82380B38` <- the script
    interpreter), on an Event "never set or released, reset 1 times 7.7 s ago";
  - `sub_8256CDC0` is the movie's play call (first read as its stop): it releases the D3D device's thread
    ownership (`sub_82173198`), sets two workers' state, resumes the three workers at +264 / +268 / +276
    (`sub_82355CF8`, NtResumeThread), then waits for the movie's end;
  - three of the player's worker threads (`sub_8256BBF8` / `sub_8256C1D8`, `sub_8256C4B8`, `sub_8256DAE0`) were in
    NtSuspendThread on themselves (`sub_82355D38`; the SDK's XThread::SelfSuspend); its control thread was in
    `sub_8255F1B8`, `while (this->+368 != 0) Sleep(50)`.
  **First guess, wrong**: a lost early resume (the Switch path drops a resume of a running thread, as the console's
  kernel does; the Win32 path keeps it as a count of 255). Keeping it changed nothing: emu5 stopped at the same press.
  **The suspend / resume log** (SDK key `thread_suspend_log`, a diagnostic, off; emu7 and the PC's `susp1`) showed
  the two the same up to the skip; then on the PC the control thread suspended itself too and the main thread resumed
  all four, while on the Switch the control thread stayed in its Sleep loop. **+368 is the XMV renderer's frames in
  flight** (at most 3): `sub_8255F630` (vtable slot 10 at `0x82135488`) counts a frame up and registers a GPU
  callback for it (D3DDevice_InsertCallback, `sub_821746E8`, function `0x8255DD00` -> slot 23, `sub_8255F2E0`, an
  atomic decrement). Renut runs those callbacks only inside its own D3D hooks once the render thread has reached them
  (and in IsFencePending, which this game inlines). At the skip no thread makes another D3D call: where the render
  thread had not reached the last frame's callback when the movie's last hook ran (Ryujinx), the count stayed at 1 for
  ever; the PC's render thread had reached it. **Fixed**: a game thread entering a kernel wait or a sleep runs the
  callbacks the render thread has reached (`rex::system::SetGuestWaitHook`, called from `ApplyGuestCoreLayoutAtWait`
  on game threads; Renut sets it at the first callback; key `banjo_renut_callbacks_at_waits`, on), as the console's GPU
  runs them by interrupt. The early-resume change was taken out again (console semantics). Handoffs
  `05_callbacks_at_waits.patch` and `06_thread_suspend_log.patch`.
- **emu8** (200 s, the fix): through the 144 s press at 60 fps, no stall dump.

## SMAA (2026-11-19, the battle's draw log: `banjo_renut_diag_draw_log_s = 590`, PC `out/pc_runs/runA`, Ryujinx `out/emu_runs/rdcemu1`)

A frame of the board battle is 238 draws on both: draws 0-236 with one shader pair (VS `8F1CFE22`, PS `50216CC1`, the
two compiled from `data\shader.fx`) into the 1280x736 target, the 3D board and the HUD alike (only the depth control
changes between groups: `00700736` with depth, `00700712` without), then one rect-list copy with the same pair into the
1280x720 front buffer. There is no composite before the HUD for `banjo_renut_smaa_after_ps` to name: naming
`50216CC1` would run SMAA after the frame's first draw; the copy is after the HUD. The base's default hash (N&B's
`981A6A58F2ABFA42`) never matched here ("SMAA ... 0 frames" in every log). **`banjo_renut_smaa = false`** in the
built-in defaults and the shipped tomls: the picture is the game's own (the request's pixel-identical default).

## Audio (2026-11-19, `audio_channel_census = true`, PC `out/pc_runs/runA`, 65 windows of 10 s: boot to the battle)

The speaker configuration reported is the default `0x00010001` (analog stereo, digital Dolby Digital): the game still
put sound in **FL and FR only** in every window (C, LFE, RL and RR 0 frames in all 65). It mixes stereo whatever the
configuration says, so the port's 5.1-to-stereo fold has nothing to fold. `audio_speaker_config = 1`: see below.
`audio_switch_pump_pairs` is the user's choice (off as the base ships it).

## Missing 2D panels on the board in Ryujinx (2026-11-19): the fast draw's declaration, fixed

- **What**: in emu9 (the full route, 840 s, the build with the callback fix), from the map's load (452 s) on, the map's
  2D panels were wrong: the "Santana Village" sign drawn as a translucent red rectangle, Faustina's dialogue box and its
  text missing (the portrait drawn), and in the battle the Cepter and Creature Card panels and "Spell Selection"
  missing while the hand of cards drew; the user saw it live too ("there's meant to be a textbox and some image is
  flickering"). The PC (Vulkan) never showed it, nor did rdcemu1 (with RenderDoc) or emu10 (with the texture memo's
  verify) on the same NRO: **1 of 3 Ryujinx runs that reached the board**.
- **What it is not**: the draws. The battle's draw log is the same on the PC and in Ryujinx (238 draws, one shader
  pair, the same states); rdcemu1's RenderDoc capture of the battle has every panel in the game's 1280x736 target and
  in the front buffer; emu9's verify counters (store cards, fast draw, whole buffers ...) were all 0.
- **What it looks like**: textures of the map's UI (loaded at the map's start into memory the story's textures used)
  answered with other contents: a red tile for the sign, transparent for the dialogue box, panels and font. The two
  shortcuts that would keep a texture bound across such a change are deko3d's texture-bind skip
  (`banjo_deko3d_texture_skip`, A/B 39: no rebind while pipeline, descriptors and texture words are unchanged) and the
  fetch-only texture memo (`sasrt_renut_texture_memo_fetch_only`, keyed by the fetch words). Not proven: the bad state
  did not come back with diagnostics on.
- **Left for the console**: `configs/renut_textures_plain.toml` turns both off; if the console shows the missing panels
  with the shipped settings, that toml is the first trial, and a RenderDoc capture of Ryujinx in the bad state
  (`tools/run_ryujinx_rdc.ps1 -Times "455,485"`) would name the texture.

`audio_speaker_config = 1` (analog stereo only; PC `out/pc_runs/poison1`, 300 s: boot, the title, the story): the same,
FL and FR only in every window. The game's mix does not depend on the configuration: the shipped default
(`0x00010001`) stays.

- **Found (07:35)**: emu11 (the copies build) showed it again: 2 of 4 runs. The two good runs both had the draw log
  on, and in `FastDrawLocked` the draw log (`cfg.draw_log`) sends every draw down the full path
  (`FastMiss(kFastDiag)`): **the good runs ran without the fast draw** (`banjo_renut_fast_draw`, A/B 25, deko3d
  only: `!IsDeko3D()` returns at once, so never on the PC), the bad runs with it ("100.0 % fast" in their logs).
  The fast draw starts each draw from the draw before and redoes only what the recording's field bits and the render
  thread's epochs say changed; the map's UI textures, loaded at the map's start where the story's had been, kept the
  textures they replaced. **`banjo_renut_fast_draw = false`** in the built-in defaults and the shipped tomls (A/B 25
  taken out of `renut_ab.toml`); confirmation run emu12 below. Which epoch misses the change was not found (for the
  base: a handoff note). `configs/renut_textures_plain.toml` stays as a further console trial.
- **Cost of the fast draw off** (emu12, Ryujinx, a proxy): the render thread busy about 15-16 ms a frame on the board
  and the story's scenes under 60 fps (39-52) in Ryujinx; the fast draw is the render thread's main saving, so the
  fault was looked for rather than kept off. **`FastDrawLocked`** recomputes a draw's textures only when the
  recording's field bits (`kFieldFetch`, `kFieldTexWords`), an epoch or the frame say so; this game links D3D9LTCG
  (Direct3D's setters inlined into the game), so a texture change between two draws of a frame need not pass a hook.
  **`banjo_renut_fast_fetch_check`** (on): each sampled slot's six fetch words and the device's texture pointer are
  compared with the last fast draw's, and a difference redoes the textures; the 10 s fast-draw line counts the draws
  it caught ("fetch words changed with no field bit"). It only adds recomputations.
  **Result (emu14, the fast draw on with the check)**: the panels broken again (3 of 3 runs with the fast draw) and
  the check caught **0** draws in every window: the slots' fetch words and texture pointers do not change unseen, so
  that is not it. The check was taken out again (commit after 06344559). **Left open**: what goes stale is some other
  state the fast draw keeps when no field bit says it changed; the strongest lead is the dirty tags the game's own
  inlined Direct3D setters set (`m_Pending` words 2-4, read as marks: `d3d_native.h`, `kStateTagChunks`), whose
  words and bits may sit elsewhere in XDK 2.0.3424 than in the XDKs Renut was written for (a `` bool left
  stale would draw a panel untextured: a coloured translucent box, as seen). **Shipped: the fast draw off**, the
  picture right in 3 of 3 runs; its cost is the render thread's (a proxy: about 15-16 ms a frame in Ryujinx, the
  story's scenes under 60 fps there; the console decides).
- **Found (2026-11-19 09:30, the user's request to fix the fast draw)**: a diagnostic key,
  `banjo_renut_diag_fast_force` (`from-to:mask;...`, seconds after the first fast-draw check: field bits every fast
  draw takes as changed in that window, logged as `[fastforce]`), bisected the kept state in one run each:
  - emu16: every field forced: the dialogue boxes back; none: missing; the textures (22001), the bools (2), the view
    packets (3C): missing; the rest (1DFC0): the battle HUD back.
  - emu17, one bit a window in the battle: **`kFieldDecl` (100)**: the HUD back in both captures; `kFieldShaders`
    (8000), which recomputes the declaration too: back once; every other bit: missing.
  - The cause: the game draws its 2D panels with an FVF (SetFVF), and Direct3D builds that vertex declaration inside
    the device (`kDeviceDeclarationOffset` 0x2E90 here, its elements from +52) and points the declaration word
    (0x2D10) at it. A new FVF rewrites the declaration in place; the word keeps its address, so no `kFieldDecl`, and
    the fast path (which looks at the declaration again only on `kFieldDecl` or a shader change) kept the draw
    before's vertex layout: the panels' quads read with another layout. The full path (`DrawStateLocked`) reads the
    declaration at every draw. The device's own declaration's bytes mapped to `kFieldTail`.
  - **Fix**: `FieldOfOffset` (render/draw.h) counts those bytes (0x2E90 + 52 + 15 x 12) as `kFieldDecl` too, with a
    static_assert against `kDeviceDeclarationOffset`. Handoff `07_fast_draw_device_declaration.patch` (the base's
    offset 0x2F98).
  - **Confirmed**: emu19 (the fast draw on, 100 % fast, nothing forced): every panel right through the route (the
    dialogue boxes, the card panel, "Drew 1 card", the battle HUD), no stall; the render thread about 7-10 ms a frame
    there (a proxy; 15-16 ms with the fast draw off). (emu18 does not count: started while another session compiled,
    its boot ran at 0.5-17 fps and the timed route ended on the title screen.) The fast draw is on again, built in and
    shipped; A/B 25 back in `renut_ab.toml`.
  - **emu20** (the built-in defaults, no fast-draw key; commit 862b5706): the sign, the dialogue boxes, the card
    panel and the battle HUD right for the whole route, 100 % fast draws, no stall, every verify line 0.

## Hot functions: the machine copies (2026-11-19)

- **Profiles**: Ryujinx (`out/emu_runs/emu9/rex_profile.log`, the ARM sampler over the whole route, with that NRO's ELF
  kept next to it) and the PC (`banjo_pc_profile_s = 60`, `banjo_pc_profile_top = 400`, `out/pc_runs/runA`, 600 s to the
  battle). The game is light: on the PC the guest Sleep wrapper `sub_82357230` (25 % of busy samples, waiting) and a
  short list of engine functions (`sub_82387670` 1.7 %, `sub_823820B0` 1.5 %, `sub_823885B0` 0.7 % ...); in Ryujinx the
  main thread is 37 % generated code, the render thread's work is Renut's.
- **List** (`app/src/cpu/hot/localized_bulk.txt`): 56 from the Ryujinx profile (`tools/switch_hot_bulk.py --merge`,
  min 1 sample; 38 under 40 instructions left out, 19 mentioned by a source left out) and 43 from the PC's
  (`tools/pc_hot_to_bulk.py`, 300 samples or more, the Sleep wrapper skipped): **99 copies** (12 shards), all made by
  `localize_native.py` (none refused).
- **Checked on the PC** (the hooked build, guard forms on): `verify2`, the route to the battle with
  `banjo_cpu_verify`, `banjo_hot_guard_calls = 4000`, `banjo_hot_guard_period = 1`: 170,431,758 calls compared,
  **0 differences**; every other verify line 0. `poison1` (`banjo_hot_poison_dead = true`, period 64): 0 differences.
  The Switch build has them in place (SASRT_HOT_IN_PLACE, 202 `g_in_place_` symbols).
- **Register diet round 5**: the 47 functions of the compiler's exclusion set that either profile sampled (shared
  helpers: `sub_82374F48`, `sub_823A7DD0` ...; 31 % of the Ryujinx gameplay samples were in the set) are back in the
  diet; 3,410 stay out. Both shaders compile the same (`verify2`, `poison1`).
- **CG-13 again** (`verify2`, the census on the same run, 63 windows): the same sites as before, VMX
  (`vmsum4fp128` in `sub_8228BF08`, now reached through the copies of `sub_82291548` / `sub_8238A7D0`; `vmulfp128` in
  `sub_823820B0`; `vmaddcfp128` in `sub_825A61A0`), the start-up self-check, and the float-bits hash
  (`sub_825183E8` / `sub_824AD460`). **BLUR_FPCR_ONE_MODE stays ON.**
- Not done: a console profile (the only source for hand-written natives, the request), more batches of unsampled
  eligible functions (a PC build with the copies took 40 min).

## Enhancements (2026-11-19)

- **Checked with the verify keys** (`configs/renut_verify.toml`: fast draw, resource objects, fetch cache, whole
  buffers, persistent shared, chain reuse, snapshot deltas, store cards, record extents; plus `banjo_cpu_verify`):
  Ryujinx emu2, emu9, emu10, emu11 and the PC's verify2 to the battle: **0 differences** in every verify line. One
  "fast draw verify" line in emu1 (the key the same, only the pipeline object different: a provisional one replaced).
- **Off-by-default.md section 1** (fixes that break another game): none turned on (no A/B showed a need).
- **Section 4 trials, left off** (as the base ships them; for the console): `vivapinata_shader_noearly`,
  `audio_xma_kick_without_lock`, `BLUR_LOCALIZE_PROMOTED_MEMBERS`, `audio_xma_kick_wake_idle_only`,
  `switch_yield_spin`, `banjo_display_sync`, `banjo_renut_core_layout`, `banjo_renut_deferred_chain`,
  `banjo_deko3d_eager_kicks` / `banjo_deko3d_latency_trim` (eager kicks is on in the base's shipped toml: unchanged),
  `banjo_deko3d_skin_once` / `light_branch` / `gpr32` (Banjo's shaders only: nothing here), `banjo_gpu_zcull_greater`.
- **Three keys from the log**: `ultimate_renut_replay_inherit_same = false`, `sasrt_gpu_skip_inert_draws = false`,
  `banjo_renut_tags_check = 256` ("Keys from the log").
- **New here**: `banjo_renut_callbacks_at_waits` (on: the movie skip), `banjo_renut_smaa = false`,
  `RENUT_ALPHA_TEST_UNORM8 = 1` (compile-time: the models' alpha test).

## The package's own settings in Ryujinx (2026-11-19 10:39-11:26)

- emu21 (the rc2 package's `culdcept.toml` and DKSH cache, 240 s, no input): the app stopped 4 s in, inside
  `banjo::profiler::Start` (renut_app.h, OnPostSetup): the Switch CPU profiler wrote its start-up header and then
  nothing (no 10 s window), the stall watch (started right after it) never started, no "Switch: in focus", the game
  never began. The likeliest place is the stack sampler (`switch_profile_stacks = true`, the base's shipped value: it
  pauses each thread every 1 ms to read its stack) left with a thread paused. Not seen again: emu22 (the same toml
  plus `headless`), emu23 (plus SMAA on) and emu24 (the same toml unchanged) all booted to the title with their audio;
  every route run (all with the sampler on) started too. **Left as the base ships it**, written down for the console:
  a black screen at boot with no log past the "[enhancements]" lines is this; `configs/renut_play.toml` has the
  sampler off.
- emu24 (the shipped toml unchanged): the D3D device, focus, the game's audio, frames: the shipped settings boot.
