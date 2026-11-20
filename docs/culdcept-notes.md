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
  every route run (all with the sampler on) started too. Left as the base ships it at first, written down for the
  console: a black screen at boot with no log past the "[enhancements]" lines is this.
- **The user's decision (2026-11-19 afternoon): the profiler on, its stack sampler off** in the shipped toml
  (`switch_profile_stacks = false` in `tools/make_culdcept_tomls.py`'s SHIPPED, so in renut.toml, renut_play, renut_ab
  and every config except `configs/renut_profile.toml`). What stays on: the SDK's Switch profiler thread (FPS, CPU and
  the counters in `logs/rex/rex_profile.log`: its "Stack sampling: disabled" mode), the stall watch, the GPU profile
  lines. What goes: the 1 ms pauses of every game thread. Package rc3; emu25 (below) boots it.
- emu25 (the new shipped toml, 180 s, no input): "Switch: in focus" at 1 s, the D3D device, the intro; rex_profile.log
  "Stack sampling: disabled (1 ms)" with its windows written, 15 frame-pacing windows (about 58 fps once past the
  logos), no `[stall]` line, every verify line 0 differences.
- emu24 (the shipped toml unchanged): the D3D device, focus, the game's audio, frames: the shipped settings boot.

## The Cepter name on the console (2026-11-19 afternoon, the user's console tests)

- **Symptom** (the user, package rc3): naming the Cepter looped (the keyboard came back after it closed), then the
  name never showed in the field and the cursor froze. Ryujinx with the Switch keyboard (`RYUJINX_SWKBD_AUTO`, the
  local Ryubing's test patch; kbA) showed the freeze too; the earlier route runs never met it: `headless` answered
  every system UI at once.
- **The game's code**: the name task `sub_822B8618` (step at +40, kind at +44, its result byte at base + 0x14F7B,
  base = `sub_822DEC80()`) opens the keyboard object (`sub_822AFC20` / update `sub_822AFF18`: XGetOverlappedResult 0
  -> read the buffer, 1223 -> cancelled (255)), checks the name 2 s later (empty -> message 17, a banned word
  (`sub_822AD100`, two tables at 0x8283BF90 / 0x8283CB98) or all spaces (`sub_822F6DA8`) -> message 28, both back to
  the keyboard 2 s later), then asks "This Cepter's name will be ..." with **XShowMessageBoxUI** (`sub_822B0098`, Yes
  focused); Yes -> the name in the field, No -> the keyboard again.
- **Two faults, two SDK fixes** (base handoffs still to make: 09 and 10):
  1. A cancelled keyboard completed with ERROR_SUCCESS and only the extended error ERROR_CANCELLED (Xenia's): the game
     read the cleared buffer as an empty name and asked again, so B could not leave the screen.
     `xam_keyboard_cancel_result` (true): the result is ERROR_CANCELLED as well. Console 16:15: the game took it as
     cancelled (255).
  2. The SDK's ImGui message box was never seen on the console: its overlapped never completed (the trace: XN_SYS_UI on
     at the box, never off), so the game held its system-UI state and took no input. `switch_message_box_focused`
     (true, Switch): a message box is answered at once with the button the game focused (as `headless`), and every box
     is logged (`XamShowMessageBoxUI: "<title>": "<text>" [buttons], focused button N`). Console 16:36: three names
     accepted ("Yes"), the field filled, the story and the board played for 31 minutes. The player does not see the
     question; a native Yes/No for the Switch is a follow-up.
- **Diagnostic**: `xam_keyboard_trace` (off): after a keyboard closes, the game's keyboard object, buffer, stored name,
  result byte and the task's first 64 bytes every 50 ms for 10 s, on the dispatch thread. (The first form made a
  std::thread there and ended the program on the console: `std::thread::detach` threw under libnx; a lesson for any
  SDK code on the Switch.)
- **Ryujinx note**: two Ryujinx runs (kbT, kbT2) froze at boot while other sessions held the PC's CPU at 100 %; one had
  the stack sampler off, so emu21's boot stop was the PC's load, not the sampler (the user's choice of the sampler off
  stands).

## Console performance (2026-11-19 16:36-17:11, handheld, the user's 31-minute session, package rc3 + the fixes)

From `out/console_runs/user5` (renut_005.log, rex_profile.log; the stack sampler off, so no hot functions):

| Scene (by draws a frame) | Windows | fps mean (min) | 1 % low (median) | Render thread | GPU | Draws/frame |
|---|---|---|---|---|---|---|
| Board / battle (120+) | 122 | 57.7 (52.8) | 10.4 | 4.5 ms (p99 max 7.9) | 7.3 ms (max 13.6) | 246 |
| Field / story (40-120) | 41 | 58.8 (49.1) | 12.2 | 1.6 ms | 5.3 ms | 76 |
| Menus (2-40) | 13 | 58.3 | 58.1 | 0.7 ms | 3.1 ms | 17 |
| Movies (<2) | 3 | 48.9 | 28.8 | 1.4 ms | 1.2 ms | 1 |

- **The GPU has room**: 7.3 ms of 16.7 on the board at 460.8 MHz (handheld); 4.0 ms of it the board's one blended
  pixel shader (PS 50216CC1CA6DFDD2) at 1280x736, 0.9 ms resolves, 0.5 ms present. Docked (1080 lines, 768 MHz) not
  measured.
- **The frame rate's losses are the game thread's own hitches**: 517 of 519 board frames of 50 ms or more had no wait on
  the renderer and short render / GPU frames (3-8 ms); none had a slow read or open of the SD card near it (300
  `[io] SLOW` lines, none within a hitch). About 6 frames of ~90 ms every 10 s on the board account for the whole gap
  from 60 fps (6 x 73 ms lost ~ 26 frames a window: 57.4 fps, measured 57.7). The game thread otherwise waits for the
  renderer's frames-ahead limit 167 ms a second: the ordinary frame is well inside 16.7 ms.
- **CPU**: the process 91 % of 400 % on average (154 % peak); the game's main thread 45 % of a core (90 % peak), the
  render thread 20 % (36 %), the rest under 6 %; the XMV movie decoder ~97 % of a core while a movie plays (the three
  movie windows, 48 fps presented). Clocks CPU 1020 / GPU 460.8 / memory 1600 MHz, SoC 34-39 C. The user: the CPU clock
  is not to be changed.
- **Next for the hitches**: one board session with `configs/renut_profile.toml` (the stack sampler on) names the main
  thread's hot functions in them, for machine copies (the hot-function method) or a fix in the game's own code path.

## The dialogue box's line (2026-11-19 evening, the user's report)

- **Symptom**: a faint white vertical line in the cutscenes' dialogue box (x 461 at 1280x720, the box's full height),
  on the console and on the PC with Renut alike (the user's Faustina screenshot; out/pc_runs/story1 too).
- **Reproduced** from the user's own save (`out/user_save`, read over FTP; never in git): Load -> Continue -> To Next
  Stage (Yes) -> the Colosseum's Faustina scene (out/pc_runs/dlg_hpo_on3). `seam_scan.py` (the session's scratchpad):
  column 461 4-9 levels above its neighbours in every dialogue frame.
- **Cause**: `banjo_renut_half_pixel_offset` (the viewport moved half a pixel for D3D9 pixel centres, PA_SU_VTX_CNTL
  bit 0 clear: device word 0x2940 is right for this XDK, the game's own setter `sub_8217A768` writes it with dirty bit
  35 as Renut's does). For Culdcept's 2D quads it puts the art half a texel off: the box's seam sampled the texel beside
  it, and the text and portraits were softer.
- **Fix**: off for this game (built-in defaults and every shipped toml). Measured on the PC (dlg_hpo_off): column 461
  20.1 against 20.1 / 20.0 beside it (28.0 against 20.0 before); horizontal detail of the text +5 %, of the portraits
  +12 %, of the 3D +3 %. The Banjo case the key was made for (a one-sided blur's halo) has no counterpart here.

## The battle's end fade (2026-11-19 evening, the user's report; CLOSED: the game's own look)

- **Symptom**: as a battle fades back to the board, parts of the battle scene vanish (the river's near slope, a lava
  pillar), the water plane shows over the rocks and the rocks' backs show through (the user's three clips; PC Renut
  the same, Vulkan and D3D12).
- **Reproduced** with the user's replay file (main menu -> Replay; out/user_save) on the PC, ~157-165 s into the run,
  recorded with `banjo_record_s` (fade_renut, fade_log1-4) and logged with a new window for the draw log
  (`banjo_renut_diag_draw_log_s` .. `_until_s`, one frame in `_every`; clears and resolves logged too).
- **What the game sends in a fade frame** (out/pc_runs/fade_log3, frame 15767; log4 with resolves): clear all; the
  board's 140 draws (opaque order, depth written); two depth-only clears (flag 0x10 = Z in Renut's and the XDK's
  convention); then the battle backdrop twice, every mesh alpha-blended (SRC_ALPHA / INV_SRC_ALPHA, the second set's
  two waterfalls additive) with RB_DEPTHCONTROL 0x712 (Z test LESS, no Z write) and colour mask F, in an order unlike
  the battle's own (sorted); three more depth clears; two quads; the resolves (0x300, colour and depth cleared after) and
  the composite. In battle frames the first set is 0x736 (LEQUAL, Z written). So in the fade the battle meshes do not
  hide each other: the picture is the draw order.
- **Not an enhancement**: the same with `banjo_cpu_enhancements` and `banjo_noncpu_enhancements` off (fade_off2).
  Renut reads the depth control and colour mask straight from the device's packets (m_ControlPacket.DepthControl,
  m_ValuesPacket.ColorMask) for each draw.
- **Closed (2026-11-19 19:10)**: the user checked on real hardware: the Xbox 360 shows the same fade (the battle scene
  drawn translucent with no depth among its meshes). Renut draws what the game sends; nothing to change.
- **Before that, no reference**: the native renderer refuses Culdcept (Banjo TU3's device layout: it falls back to Renut), and
  the SDK's emulated Xenos never reached the title in 330 s. A recording of this fade from the console or Xenia would
  say whether the Xbox 360 shows the same; candidates if not: the first set's Z write (a depth pre-pass the device state
  does not show), its depth bias, or the per-mesh fade constants.

## Local multiplayer: a second controller could not join (2026-11-19 evening, the user's report)

- **Symptom** (console, the handheld plus a Pro Controller): in Versus, "Press A to join" took player 1 but never
  player 2; once player 1 had joined twice, player 2's stick and D-pad did nothing. On the PC with scripted second and
  third pads (`input_script_p2` / `_p3`, new test keys): player 2 joined only when a third pad was signed in, and
  player 1 alone could not join at all.
- **Traced** with `xam_user_trace` (each sign-in query with its guest caller) and `debug_watch_u32` (guest words logged on
  change: the input manager 0x82D7CAC4, its active pad +100, its per-pad records +108 + pad x 20): the lobby's handler
  (`sub_82248AB8`) asked `sub_822FB888` (signed in?) for pads 0-3 at once when only pad 0 pressed A, so every pad read A.
- **Cause**: the game polls each port (`sub_82389E28` through `sub_823555B8`) and ignores XamInputGetState's result;
  it reads the state buffer, a stack slot the previous port's poll had just filled. The SDK returned
  DEVICE_NOT_CONNECTED for an empty port without touching the buffer, so every empty port read as a copy of the port
  polled before it. The lobby's "which pad pressed" takes the last one: an empty, signed-out port, so no join (player
  1 alone: ports 2-4 copied player 1; two pads: ports 3-4 copied player 2).
- **Fix** (SDK, handoff to come): `xam_input_clear_disconnected` (true): XamInputGetState clears the state of a port with
  no controller, as the console does. PC: player 2 joins, the game asks it for its own save ("Begin with this data?"),
  its D-pad moves its cursor; player 1 alone joins. On the card at 20:29 for the user's test.
- **Also**: `xam_guest_signin` now signs in a pad that connects after the game's sign-in screen (the game told with
  XN_SYS_SIGNINCHANGED); `xam_signin_ui_opens` (off) was a first guess, kept as a key.
- **Console check (2026-11-19 22:58-23:07, the user's request; sys-botbase "2.6-files-kill-pads", tools/switch_remote.py
  `--pad N`)**: in the console's Versus lobby, virtual pads 1-3 (the game's controllers 2, 3 and 4: the handheld is
  controller 1) each joined with A (the Switch's B: face buttons by position), loaded the save and took slots 1-3
  ("Chanseys" with pad icons 2, 3, 4). A fourth virtual pad is a fifth controller (no port for it); the handheld's own
  join (controller 1) was the user's earlier test. Screens: out/console_runs/mp4.

## The movies through FFmpeg (2026-11-19 night, the user: "can you do the wmv decoder now please", "the native one")

- **The movies**: data\MOVIE\*.wmv are ASF with WMV3 video (1280x720, 30 fps for CI_00-02 and the credits, 60 fps
  for narration_* and complete_*; no B pictures) and WMA2 audio, played by the XDK's movie player with Microsoft's WMV9
  decoder in guest code (~97 % of a console core while a movie plays; the movies presented at ~48 fps). Mapped by the
  sasrt-nx session's read-only notes (its scratchpad's culdcept_xmv_report.md; the method is sasrt-nx's and tw2de_nx's).
- **The hooks** (`app/src/cpu/culdcept_wmv_ffmpeg.cpp`, key `culdcept_wmv_ffmpeg`, 1): the codec interface's decode
  `sub_8259EE00` (the frame's chunks taken through the guest's GetData `sub_8259EA38` until its last, as the guest's
  decoder takes them, so the pts and the payload's release stay the guest's; FFmpeg's WMV3 decoder with the guest's
  sequence header, V+84 / V+88), get output `sub_8259EE60` (FFmpeg's YUV420P written into the player's I420 buffer:
  Y pitch W, then U and V pitch W / 2), flush `sub_8259ECF8` and destroy `sub_8259ED50`. All are reached only through
  the codec's table (no direct callers). Mode 2: the game's decoder runs and is shown, FFmpeg decodes the same bytes
  (captured at the stream node's fetch `sub_8256FEE8`) and the planes are compared.
- **FFmpeg**: the SDK's copy already compiled the WMV3 / VC-1 sources ("fork addition") but never switched them on:
  `sdk/thirdparty/FFmpeg/config.h` now includes `config_masseffect_wmv3.h` (an untracked base input, force-added);
  the app links libavcodec / libavutil.
- **PC check**: mode 2 on the intro (I and P frames): every frame bit-identical (0 samples differ in Y, U and V). Mode 1:
  the three intro movies (171, 200 and 100 frames) all decoded, 0 failed, 0 damaged; the title as before.

## The board stutter, profiled on the console (2026-11-20 00:01-00:07, the user's replay, `banjo_slow_frame_profile_ms` 40)

Console (the FFmpeg build, CPU 1020 MHz), the replay's board and battles, 4 minutes with the app's sampler and the stack
sampler on (out/console_runs/prof1; named with `tools/switch-cpu-profile.py` and that build's ELF):
- **How often**: ~230 frames of 40 ms or more (about one a second on the board; the longest 430 ms), the board's 1 % lows
  ~10 fps while the mean holds 58 fps.
- **Threads**: the game's main thread 35 % of a core on average (68 % peak), Renut's render thread 16 % (33 %, core 1),
  each game worker under 5 %, audio ~1 % (core 3); the process 80 % of 400 %; the system core 56-64 % idle.
- **Inside the stutter frames** (the main thread's samples in them): 18 % file system calls (fsdev open, its timestamp
  query and close: the game opens the same archives again and again during play, DATA00.RSP 11 times, the layout*.RSL
  files, the se_*.xma sounds; ~4-5 ms an open, the worst over 100 ms); ~22 % one call chain of game code
  (sub_822BC1B0 -> sub_82346270 -> sub_8234C090 -> sub_8234DED8 / sub_82353820 / sub_823507C0: integer bit-shifting
  loops, by its shape the data just read being decompressed); 9 % sub_82374F48 (a first-fit walk of a linked free list:
  the game's allocator, 8 % of the main thread overall); 6.5 % sub_82380170; and waits: the render thread's queue
  (6 %), the input read's latency sleep (5 %), Sleep (6 %).
- **Levers**: keep the game's archive files open (an open costs several SD-card IPCs; the [io] lines count 22-37 opens
  every 15 s); machine copies of the decompressor chain and the allocator walk (the hot-function method). Done the next
  night, and two of the readings above corrected (the next section).

## The board stutter, worked on (2026-11-20 night, the user: "the 22% and 9% functions can all get hand natives make machine functions of everything else that doesn't have them let's also do 1. keep the data files open")

What the profile's functions are (read from their code, prof1's stacks):
- **The 22 % chain is a JPEG decoder**, the library the game links (its structures at the library's offsets): sub_8234DED8 is
  the Huffman decode of one MCU, sub_8234DB88 its bit-buffer refill, sub_8234DCF8 the slow decode past 8 bits,
  sub_82353820 the integer inverse DCT of one 8x8 block, sub_823507C0 the YCbCr-to-RGB conversion of a row,
  sub_822BC1B0 the game's caller. The board decodes card art as it shows it.
- **sub_82374F48 is the game's small-block free** (from its free, sub_82375178, for blocks of 128 bytes or less): it walks
  the pool list from the head to the first pool whose first word is not zero and links the block there. Its time is the
  walk, one dependent load per pool; a native would be the same loop (the machine copy already has it, with a prefetch).
  Not given a native: nothing in it can be removed without changing the game's allocator.
- **sub_82380170 waits**: the main thread polls a status word (sub_8237A338 reads it from a table) until the game's loader
  thread finishes a read. Its share is waiting for a file; faster file access is what shrinks it.

Done:
1. **The data files kept open** (SDK, `filesystem_keep_open`, default on; `filesystem_keep_open_max` 32): a read-only open
   of a read-only device leaves its host handle parked when the guest closes it, and the next open of the same file takes
   it back (no open, timestamp or close request on the Switch). The size query of such a file no longer restats it (the
   files cannot change). PC, the board replay (verify4, 300 s): 595 opens took a parked handle, 726 closes parked
   theirs, 100 closed beyond the cap (the game rotates through more than 32 files), 54 size queries without a stat;
   the opens left cost 0.02-0.12 ms each on the PC.
   Ryujinx (rc6's NRO, emuh1, 700 s to the first battle): 713 reopens from parked handles. An open that fails while
   handles are parked closes them all and is tried once more (507786a3: the console's limit on open files was not
   measured, and a save that fails to open would be worse than a stutter). The console's numbers: when measured.
2. **Machine copies of everything in the profile that had none**: localized_bulk.txt 102 -> 256 (tools/switch_hot_bulk.py
   on prof1, `--min-samples 1 --merge`), the decoder chain and its callers among them (sub_8234DED8 and sub_822BC1B0 ran as
   their plain originals in prof1). 251 bulk copies made (3 forms the tool refuses stay generated code), 2 from
   localized.txt. PC guard on every call along the board replay (verify4): 245 of the 251 reached, 65.8 million calls
   compared, 0 differences.
3. **Hand-written natives** (app/src/cpu/hot/banjo_hand_natives.h, hand.txt with the evidence):
   - `culdcept_hand_idct` (sub_82353820): the same 32-bit integer arithmetic four columns, then four rows, at a time in
     NEON lanes (SIMDe on the PC). Tested on 200,000 random blocks (sparse, dense, DC-only, values that wrap) against a
     model of the guest instructions: identical workspace and output. PC guard (verify4, every call of the replay):
     604,800 calls compared with the original, 0 differences. Its first run (verify3) found the one register the
     native did not set: the original leaves the link register at 0x82353828 (its bl to the register-save routine);
     the native sets it now.
   - `culdcept_hand_huffman` (sub_8234DED8 with its refill and slow decode worked in): the bit reader in host registers,
     every write held until the MCU is decoded, declined where the source runs dry, a restart is due or the original would
     warn. Round-trip test of 44,924 random MCUs (1-10 blocks, random tables, stuffed bytes, zero runs, the end marker):
     decoded as encoded. The guard cannot repeat it (the refill's indirect call), so `culdcept_hand_huffman_verify` runs
     the original after it and compares: PC (huff1, the board replay), 600,000 MCUs checked, 0 differences. In the
     guard's logged form it declines (verify3, with it running there too, reported the paths as different: the native
     reaches no call where the original's statements reach the refill's), so the guard counts the hook as not
     comparable (verify4: 201,600 calls, 0 differences).
   - **Ryujinx** (rc6's NRO, the real Switch build: GCC and NEON; emuh1, the route to the first battle): the Huffman
     decoder's check 16,384 MCUs, 0 differences; the hand's card art clean in the captures (the IDCT's NEON path).
   - Not given one: the colour conversion (four table lookups a pixel; a native would make the same lookups), the free
     and the wait above.
   - **A hand.txt function needs a line in localized.txt.** tools/localize_native.py takes a hand.txt function out of
     the bulk list, and places a native in front of a callee only where a caller's copy inlines it (these two are called
     for real: the decoder's refill has an indirect call). With only hand.txt and the bulk lines, the first codegen made
     neither native's call (no `n_<id>::Native` in app/generated/hot); both are listed in localized.txt now, as
     Katamari's were.

## The board stutter, profiled again on the console with rc6 (2026-11-20 13:56-14:04, the user: "yeah you can use the console to record a profiled run just use the repla feature to play my existing run please")

The same as prof1: the user's replay (main menu -> Replay, the replay of 2026-11-19 07:03), `configs/renut_profile_slow.toml`
(identical to prof1's toml), handheld, started with sys-botbase, the replay played 4 min 34 s, closed from the Home menu,
the shipped toml put back (out/console_runs/prof2; named with rc6's ELF, the build's own, md5 cf397d82...).
The card's `banjo_profiler.log` is appended to, not replaced: prof2's copy starts with prof1's whole log, so the run's
part is `banjo_profiler_run.log` (13 reports); `rex_profile.log` is replaced each run.

**Not like for like: the memory clock.** prof1 ran at CPU 1020 / GPU 460.8 / memory 1600 MHz, prof2 at 1020 / 460.8 /
**2733** MHz (the rex_profile.log clock line, every window). Nothing here changed a clock; it was set on the console
between the runs. The GPU's time a board frame fell 5.49 -> 5.02 ms, and none of the changes touch the GPU, so part of
what follows is the faster memory. The counts of file-system requests do not depend on it.

| The replay's 9 report windows | prof1 (the FFmpeg build of 2026-11-19 23:37, memory 1600) | prof2 (rc6, memory 2733) |
|---|---|---|
| Frames of 40 ms or more of game-thread work | 313 of 15,374 | 263 of 15,631 (-16 %) |
| Time in them | 24.1 s | 17.9 s (-25 %) |
| The longest | 430 ms | 394 ms |
| Board / battle windows: mean fps, 1 % low median | 58.3, 9.7 | 59.3, 12.1 |
| Main thread in file-system requests (whole run) | 1,972 samples | 856 (-57 %) |
| ... of them in the stutter frames | 2,091 of 11,498 (18 %) | 1,093 of 8,576 (13 %) |
| The JPEG IDCT (whole run) | 1,212 | 839 (-31 %) |
| The JPEG Huffman decode (whole run) | 1,441 (without its refill's own samples) | 1,331 |
| The small-block free sub_82374F48 (whole run) | 9,321 | 7,803 |
| The JPEG chain's share of the stutter frames | ~23 % | ~23 % |

- **The file cache on the console**: 878 opens took a parked handle, 1,211 closes parked theirs, 302 were closed beyond the
  cap of 32 (the game rotates through more files), 194 size queries without a stat; **0 failed opens** (the retry of
  507786a3 never ran: 32 parked handles stay inside the console's limit).
- **What file time is left** in the stutter frames: opens made by `VirtualFileSystem::ResolvePath` itself (5.7 %, and
  its timestamp query 1.4 %: the path lookup opens the file before HostPathEntry::Open, which the cache does not cover),
  reads (2.8 %) and closes of handles pushed out past the cap (2.0 %). The next levers: the path lookup answered without an
  open on a read-only device, and a larger cap.
- **The JPEG natives** on the console: the IDCT a third cheaper; the Huffman decode about the same as the machine copy's
  (its time is the bit reader's loads and branches either way). The chain stays about a fifth of the stutter frames.
- No crash, no error in the log; at the end the replay was still playing on the board (t7.jpg), the creatures, tiles and
  numbers drawn clean.

### The rerun at last night's clocks (prof3, 2026-11-20 14:17-14:26, the user: "can you rerun it I set clocks correctly")

The same steps; the rex_profile.log clock line in every window: CPU 1020 / GPU 460.8 / memory 1600 MHz, as in prof1. The
user's clock overlay was on screen this time (a small cost prof1 did not have). Banjo's session confirmed no FTP upload of
its own ran during the run.

| The replay's 9 report windows | prof1 (2026-11-19 23:37 build) | prof3 (rc6) |
|---|---|---|
| Frames of 40 ms or more of game-thread work | 313 of 15,374 | 260 of 15,590 (-17 %) |
| Time in them | 24.1 s | 17.7 s (-26 %) |
| The longest | 430 ms | 409 ms |
| Board / battle windows: mean fps, 1 % low median | 58.3, 9.7 | 59.2, 12.1 |
| GPU a board frame; the render thread | 5.49 ms; 4.00 ms | 5.47 ms; 4.07 ms |
| Main thread in file-system requests (whole run) | 1,972 samples | 889 (-55 %) |
| ... in the stutter frames | 2,091 of 11,498 (18 %) | 1,008 of 8,554 (12 %) |
| The JPEG IDCT (whole run) | 1,212 | 806 (-33 %) |
| The JPEG Huffman decode (whole run) | 1,441 | 1,243 (-14 %) |
| Opens slower than the log's threshold | 148 | 89 |

- **So prof2's gains were not the memory clock**: at the same clocks the stutter numbers are prof2's within a few
  frames; the GPU's 5.0 ms in prof2 was the faster memory (5.47 ms here, as prof1).
- The file cache: 877 opens took a parked handle, 302 closed beyond the cap, 0 failed opens.
- The stutter frames now: file-system requests 12 %, the Huffman native 11 %, the small-block free 10 %, the loader wait
  8 %, the IDCT native 4 %, the colour conversion 4 %, the game's JPEG caller 3 %.
- No crash, no error in the log.

### The file cache at 256 (prof4, 2026-11-20 15:04-15:13, the user: "let's try 128 file cache", then "can we do 256 instead try that")

The same replay at the same clocks (CPU 1020 / GPU 460.8 / memory 1600 MHz in every window), `filesystem_keep_open_max = 256`
in the profile toml (out/console_tomls/culdcept_profile_slow_keep256.toml). The user's clock overlay was on screen, as in
prof3; Sonic Racing's FTP restores ran until 15:07, during the game's start only (the replay began 15:06:25).

| | prof3 (cap 32) | prof4 (cap 256) |
|---|---|---|
| Handles parked at most | 31 | 199 (the game uses about 200 files in the run) |
| Closed beyond the cap | 302 | 0 |
| Opens that took a parked handle | 877 | 1,014 |
| Failed opens (the retry) | 0 | 0 |
| Main thread in file-system requests (whole run) | 889 samples | 659 (-26 %) |
| ... in the stutter frames | 1,008 of 8,554 | 768 of 8,449 |
| Opens slower than the log's threshold | 89 | 76 |
| The replay's frames of 40 ms or more, time in them, the longest | 260, 17.7 s, 409 ms | 257, 17.6 s, 404 ms |
| Board / battle windows: mean fps, 1 % low median | 59.2, 12.1 | 59.2, 12.1 |

- **The console keeps ~200 open files without a complaint**: no open failed, so the retry never ran.
- **The closes are gone and file time fell a quarter, but the stutter did not move**: what is left of the file time in the
  stutter frames is the path lookup's own open of each file (4.4 %) and reads (3.2 %); the stutter frames are now mostly
  the JPEG chain, the small-block free and the loader wait.

## The game menu for Culdcept (2026-11-20, the user: "hide smaa hide framerate cap set docked default resolution in menu and in game to 1080")

- No **Framerate cap** row and no **SMAA** row (app/src/game_menu.cpp, MakeSettings): `frame_cap` and `banjo_renut_smaa`
  stay in culdcept.toml (Off and false). SMAA would run on the image that holds both the board and the HUD (one shader,
  one target), so it would smear the HUD's text.
- **Docked resolution**: 1920x1080 is the menu's default choice, its help text says so in all 12 languages (it said
  1728x972, Blur's), and `banjo_renut_render_height_docked` defaults to 1080 in the code (it was 720; culdcept.toml has
  set 1080 since the first package, with `banjo_renut_display_height_docked = 1080`).
- Built for the PC and the Switch (15:14, NRO md5 5b40e303...); the menu not yet opened on either.

## The 30 fps movies in AV1? No (2026-11-20, the user: "Why dont we just usr av1 for 30fps videos?")

Four movies are 30 fps: the intro logos CI_00-02 (15.7 s, 1.1 MB in H.265) and credit_us (3:36, 33.3 MB in H.265);
the other 35 are 60 fps. On credit_us, against the WMV3 picture (PSNR, frames paired by order): H.265 at 35 % (the
installer's) 33.3 MB, 41.4 dB; AV1 by SVT-AV1 14.2 MB, 41.3 dB; AV1 by the browser (Chromium's WebCodecs, libaom real
time; test/av1_browser.html in the installer) 22.3 MB, 39.2 dB, worse than H.265 for its size. On the console (1020 MHz,
tools/switch/av1bench, all 6,470 frames, 33.3 ms a frame): the SVT-AV1 file through dav1d on one thread 18.7 ms
average, 190 ms worst, 13 % of frames late (frame-threaded: 4.5 ms, 0.7 % late, but pictures come out later than the
port's movie hook takes them); the browser's AV1 on two threads 12.2 ms, none late; H.265 on NVDEC 4.6 ms, 9.3 ms
worst, none late. The only AV1 that saves space (~19 MB, SVT-AV1) cannot be made in a browser and stutters without a
frame-threaded hook. Every movie stays H.265 (the user: "Lets do h.265 for all the videos").

## The H.265 movies on the console: slow, silent, then dropping frames (2026-11-20 night - 11-21, the user: "audio doesn't play during the video and the video has major stutters", "can you spend 12 hours independelty to get video decoding and video playback audio on h.265 working properly")

- **Slow and silent: the remux.** The intros in H.265 played slowly and without sound, in Ryujinx too (which decodes no
  H.265): the file, not the decoder. On the PC, the original CI_01 written again through the installer's lib/asf.js
  with its own frames (nothing re-encoded) played in 11.3 s instead of 6.8. The originals' muxer never splits an audio
  object across packets; the writer did (5 of CI_01's 19), and the game's player lost a split object's sound and paced
  the movie slower. It also gave each packet its first object's send time and a duration of 0. Now (lib/asf.js and
  tools/hevc_movie.py, byte for byte the same: test/movies_node.mjs on CI_01 and narration_01): each audio object whole
  in one packet (one that does not fit waits for the next packet, which it starts, and the video goes on filling the
  packet before: a new packet for each made the narrations 10 % larger), each packet's send time the original time of
  the bytes it starts with (for a re-encoded frame, of the same fraction of the original's bytes), never earlier than
  the packet before's, and its duration the time to the next. PC: the intros 5.78 / 6.76 / 3.41 s (the originals'
  5.8 / 6.8 / 3.4); the console with the 33 movies written again: CI_00 5.79 s, CI_01 6.77 s, CI_02 3.42 s, every frame
  on NVDEC, and the user: "audio plays during video properly now".
- **Dropping frames at 60 fps: the picture's copy.** The narrations (60 fps) then took 19.5 ms a frame in the game
  (out/console_logs/renut_023.log, 10 s windows): send 1.3 ms (FFmpeg waits for NVDEC's decode inside it, at 979 MHz),
  receive 0, and 18 ms copying the picture out of NVDEC's block-linear surface (av_hwframe_transfer_data). FFmpeg's
  nvtegra (libavutil's hwcontext_nvtegra.o, read in its disassembly) copies with the VIC only into a picture whose
  planes and pitches are 256-byte aligned; FFmpeg's own picture is not, so every frame took its CPU fallback ("Frame
  address/pitch not aligned to 256, falling back to cpu transfer"): the surface's lines cleaned and invalidated, then
  un-tiled by the CPU, 16 bytes at a time, on the game's busy cores. nxvideo now gives it a picture of its own: NV12
  (the surface's format), planes and pitches 256-aligned, in whole pages of its own (nvtegra maps it for the VIC each
  frame), invalidated again after the VIC writes it. The intros on the console (renut_025/026): the copy 1.4-1.9 ms a
  frame instead of 18 (send 1.2, so about 3 ms in all against the 16.7 of a 60 fps frame). The VIC's NV12 picture is
  the CPU fallback's byte for byte; nvtegra's YUV420P copy (which would have spared the player's chroma split) is not:
  its U plane comes out 255 everywhere (out of the three pictures dumped by the verify, tmp_hevc/vicdump). The verify
  on the three intros (every 30th frame, 17 pictures, one dumped with a logo in it): 0 samples differ. A 60 fps
  movie on the console (narration_01 put in CI_00's place for one start, out/console_logs/renut_029.log): 600 frames
  every 10 s (576 in the first window), 2.6 ms a frame in nxvideo (send 1.2, copy 1.4) and 0.5 ms in the player's
  copy, against 447-475 frames and 19.5 ms before. The intros' own audio is silence (-91 dB in the originals); three
  captures of a start (out/console_runs/startup_c1) are digital silence until the title music: the "loud stutter
  noise" at the start was most likely the first writer's split audio objects (half objects fed to the WMA decoder).
  Cvars:
  `culdcept_movie_vic_copy` (true; false is FFmpeg's CPU copy), `culdcept_movie_verify` (false; true compares every
  30th VIC picture with the CPU fallback's copy and logs the count that differ) and `culdcept_movie_verify_dump` (a
  folder for the first compared pair with something in the picture).
- **The clocks and the thread.** nvtegra's DFS sets NVDEC's clock every 20 frames to the bitrate it saw over wall time
  x 1.2, and the game asks for a frame only after showing the last: the clock fell until a frame took a display frame.
  nxvideo keeps NVDEC at the clock DFS starts from (its maximum, 979 MHz; put back about twice a second in a narration)
  and sets the VIC to its maximum, which nvtegra never sets (`culdcept_movie_max_clock`). nxv_decode runs at priority
  0x2C, above the game's threads (0x3B), and the thread's own is put back after (`culdcept_movie_decode_priority`).
- **Moonlight-Switch** (its source, read with the user's permission): it never copies the picture out; FFmpeg's NVTEGRA
  frames are drawn by deko3d straight from NVDEC's surface (DKVideoRenderer), and it sets no clock. The game's player
  wants the picture in its own I420 buffer, which it uploads itself, so this port copies it out, now with the VIC.

## The music and effects in Opus (2026-11-20, the user: "Yeah I think I want to rencode the audio the to opus", "Lets go with 128k", "go ahead with the opus audio work work nonstop until its done")

- **What the installer makes**: NAME.xmo for each 48 kHz XMA file in data/SOUND (`sdk/include/rex/filesystem/xmo.h`
  has the format): the XMA file's RIFF header, packet headers, a frame table (bit position, length, last bit) and its
  trailer (each file ends with a `seek` chunk), and the Opus stream (20 ms packets) made from the frames decoded in
  order by the port's own xmaframes decoder. Music 128 kbit/s stereo, effects 96 / 48 kbit/s stereo / mono
  (installer game.json `audio`). The 44.1 kHz voice clips (nv, ev, namc: 1,446 files) stay XMA, as do 36 short effects
  that are smaller as XMA. 655 of 2,101 files, 290.1 MB -> 178.5 MB.
- **What the port does**: HostPathEntry shows NAME.xmo as NAME, with NAME's size (the VFS index keeps it, as for .lz4b);
  the handle synthesizes NAME a packet at a time: header, each frame's 15-bit length field and last bit as they were,
  and a 72-bit tag (0xA55C, the sound id, the packet's index) in the packet's tag bits (the bits after each frame's
  length field that starts in it, then the end of the frame that began before; at least 104 in every packet a frame
  starts in), everything else zero. The game loads and loops it as before. XmaContext::DecodeXmoFrame reads the tag of
  the frame's packet, finds the frame by its position, and takes its 512 samples from the Opus (libopus 1.3, devkitPro's,
  on the Switch): the decoder goes on from the last frame, or after a jump (a loop) starts 320 ms before the frame (80
  ms left the first frame after a seek at 20-25 dB from the in-order decode, 320 ms at 92 dB or better, 52 seeks). The
  Opus data is read when the game reads the file (its thread, not the audio thread), up to 16 MB held.
- **Checked**: tools/test_xmo.cpp on all 655 files (every packet byte for byte against the original's header, lengths and
  last bits plus the tag; every frame found from its packet's tag; no original packet reads as a tag);
  tools/xmo_compare.py (the Opus against the XMA decode: offset 0 samples, 14-20 dB waveform SNR, normal for a
  perceptual codec); a browser-made .xmo (test/audio_browser.html) the same. Ryujinx with the user's replay (xmo3,
  xmo4): 22 sounds from Opus, no [xmo] warning; the dumped frames of bgm_46_01, bgm_01_01 and bgm_01_03 match their
  Opus at offset 0 (65-79 dB against ffmpeg's newer libopus); bgm_01_01's loop jumps back exactly 5,855 frames to
  frame 745 and matches from there.
- **Costs**: the first start after an install reads each .xmo's header for the index (Ryujinx: the full scan 39.6 s
  against 10.8 s; the console 22.8 s); later starts use the index (237 ms). The XMA worker thread on the console: 1.96 %
  of a core against 1.80 % with XMA over the replay's first 5 minutes (481 against 388 us a context), the worst
  context 4.75 ms (a loop's 320 ms pre-roll; the worker's, not the game thread's).
- **On the console** (2026-11-20 21:07-21:14, out/console_runs/xmo_c1; the .xmo files put in game_root/data/SOUND,
  the XMA files set aside, this build's NRO, the user's replay): 25 sounds from Opus, no [xmo] warning, no error; the
  title music's data read 11 ms (52 ms as XMA). Then everything put back: the XMA files, the user's rc6 NRO, the
  untouched culdcept.toml; the .xmo files left in /switch/culdcept-nx/xmo_test and the test NRO as
  culdcept-nx_opus_test.nro (the FTP tool deletes nothing).
- **The installer** (culdcept-nx-installer): the full test in Chrome made the zip with every movie in H.265 and 634 sound
  files as .xmo (290.2 -> 171.2 MB), 2,576 MB in all, checked by test/check_zip.py and six .xmo by out/test_xmo.exe.
  Its first full run kept 34 movies as WMV and 63 sounds as XMA: the worker pool posts a busy worker its next job, and
  the movie and audio workers' handlers awaited the encoder, so a second job reopened the one WebAssembly decoder under
  the first (each worker now takes its jobs one at a time).
