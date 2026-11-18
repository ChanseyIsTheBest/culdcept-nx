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

### The register diet breaks the game's HLSL compiler: off for now (2026-11-19)

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
- The diet stays off until the key and the function are found (the next bisection: each key alone, then the compiler's
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
