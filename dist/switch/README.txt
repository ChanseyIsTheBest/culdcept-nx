culdcept-nx: Culdcept Saga (Xbox 360, title 424107DD) on the Nintendo Switch
==========================================================================

A ReXGlue static recompilation built on renut_nx_ultimate_base (the base of katamari-nx, vivapinata-nx, sasrt-nx and
the other ports): its SDK fork, its post-codegen CPU passes and reNut's Direct3D-level renderer on plume's deko3d back
end (no NVK, no PM4 ring, no EDRAM model). Your own game files are needed; nothing of the game is in this package
except the recompiled code and the compiled shader cache.

Credits: reNut's renderer by masterspike52 (its author permits its reuse in these ports), renut_nx_ultimate_base and
the ports it carries, renut-nx, sa2-nx (input, CPU items), NetherSX2_nx by NaGaa95 (the four-core layout, MIT),
battd_nx (the GPU arena), bejeweled3-nx and the zuma ports (the hot-function method). GPL-3.0 where the sources are.

SD card layout
--------------
sdmc:/switch/culdcept-nx/culdcept-nx.nro      the program
sdmc:/switch/culdcept-nx/culdcept.toml        settings, the only toml the NRO reads (see "The tomls")
sdmc:/switch/culdcept-nx/deko3d_shaders.bin   the prebuilt shader cache (optional: without it the first start
                                              compiles the game's 7 shaders, a second or two)
sdmc:/switch/culdcept-nx/game_root/           the disc's files: default.xex and data/ (DATA00.RSP, MOVIE/, ...)
                                              (extract your ISO; the ISO itself is not read)
The game's saves go to sdmc:/switch/culdcept-nx/renut/. No title update or DLC is needed or read.

The tomls
---------
The settings are compiled into the NRO (app/culdcept_switch_defaults.toml), and it reads one file over them:
culdcept.toml. A toml under any other name does nothing and gives no log, so to use one of these, copy it over
culdcept.toml. Delete culdcept.toml and the next start writes the player's short default one. A build made with
-DCULDCEPT_BUILTIN_CONFIG=OFF compiles nothing in and reads renut.toml instead; tools/package_switch.ps1 names the
package's copy to match.

In configs/:
renut.toml        the shipped settings with logs on: Renut on deko3d, 720 lines handheld and 1080 docked, MSAA off
                  (msaa_level 1: the game's own count is your decision), SMAA off (the game draws its board and HUD
                  with one shader into one target: nothing to anchor it on), no dynamic resolution, every exact
                  enhancement on as the base ships it, the items measured SLOWER ON THE CONSOLE in the base's ports
                  off. A package carries it as culdcept.toml.
renut_play.toml   the same with the diagnostics and logs off (for playing).
renut_ab.toml     the same with an in-run A/B list, 30 s halves, target 1 as the do-nothing control (a Vulkan-only
                  key). Stand still in one place (a board turn waiting for input) for 10+ minutes; the log's
                  "[renut A/B]" lines give each target.
renut_verify.toml every renderer self-check on (slow; the log's verify lines must count 0 differences).
renut_profile.toml the CPU sampler and its stacks on: a console profile for the hot-function method (see Profiling).
renut_pgo.toml    the shipped settings with no log, no profiler and no diagnostics (a PGO training run's toml).
renut_720_docked.toml  720 lines docked too, the Xbox 360's own (pixel-identical to the console).
renut_msaa2.toml  MSAA 2x (changes the picture: your decision).
renut_dynamic.toml dynamic resolution, 540-720 handheld / 720-1080 docked (not pixel-identical: your decision).
renut_textures_plain.toml  two more texture shortcuts off (the deko3d bind skip and the fetch-only memo): try it if
                  the board's 2D panels (dialogue box, HUD) still go missing or show another texture. (Their cause in
                  Ryujinx was Renut's fast draw, which every toml here has off: banjo_renut_fast_draw = false.)

Cores
-----
The port runs on the three cores a homebrew launch gets. Four cores are optional and off: with
banjo_forwarder_program_id in culdcept.toml set to your own forwarder's id, the NRO writes that forwarder's four-core
NPDM (NetherSX2_nx's layout, CPU 0-3) to sdmc:/atmosphere/contents/<id>/exefs/main.npdm, and the next launch has
CPU 0-3 (the log's "[banjo] system core:" line says whether the fourth core was granted). Delete that main.npdm to go
back to three cores.

Profiling
---------
The ELF (culdcept-nx.elf) and the source snapshot (source_<commit>.tgz) are the ones this NRO was built from: a
console profile maps to source lines with them. To make one: copy configs/renut_profile.toml over culdcept.toml, play
the part to measure for a few minutes (the board, a battle), quit, and keep sdmc:/switch/culdcept-nx/logs/ (the
"[profile]" lines and rex_profile.log). tools/switch_hot_bulk.py with this ELF turns it into the next build's list of
machine copies (docs/optimising-functions.md).

What this port changes for the game (all exact unless said)
-----------------------------------------------------------
- The game compiles two of its seven shaders at run time from data\shader.fx with D3DX; both shader formats it uses
  (version 0x10 from that compiler, 0x11 from its data) are translated.
- Alpha test at the console's 8-bit precision (the models' "alpha >= 1.0" test against interpolated white).
- Everything else is the base's renderer and CPU work, listed in the log's "[enhancements]" line.
