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
| 00:20-01:00 | Renut's remap prepared: Culdcept's device layout measured equal to Viva Pinata's (XDK 2.0.3529); hook map 291 of 368 lines placed (Viva's lines, the pairs, signatures, Culdcept's own state tables); guest data addresses found; vivapinata-nx's edit and zone tools adapted; its two later XDK fixes (integer viewport, GetRenderState bits) scripted. |
