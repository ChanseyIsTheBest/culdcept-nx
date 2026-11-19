#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from katamari-nx's tools/make_katamari_tomls.py): the package's tomls, from the base's shipped
renut.toml (dist/switch/katamari-nx/renut.toml of renut_nx_ultimate_base 5017fe95: every port's defaults), with Culdcept
Saga's settings:

  renut.toml       the shipped settings: Renut on deko3d, 720 lines handheld and 1080 docked as the base ships, no dynamic
                   resolution, MSAA off (msaa_level 1, the base's default: the game's own count is the user's decision),
                   every exact enhancement as the base ships it, the items measured SLOWER ON THE CONSOLE off, the three
                   keys decided from this port's own log (docs/culdcept-notes.md, "Keys from the log"), logs on;
  renut_play.toml  renut.toml with the diagnostics and logs off, for playing;
  renut_ab.toml    renut.toml with an in-run A/B list, 30 s halves, target 1 (a Vulkan-only key: does nothing on the Switch)
                   as the control;
  configs/*.toml   the verify, PGO and picture-changing options, each one key away from renut.toml.

The NRO reads one toml: culdcept.toml (CULDCEPT_BUILTIN_CONFIG on, the default) or renut.toml (off): the package's
copy is named by tools/package_switch.ps1.

  python tools/make_culdcept_tomls.py [--base dist/switch/katamari-nx/renut.toml] [--out dist/switch/culdcept-nx]
"""
import argparse
import os
import re

HEADER = """# culdcept-nx (Culdcept Saga on the Switch, Renut's renderer on deko3d): {what}
# Made by tools/make_culdcept_tomls.py from renut_nx_ultimate_base's shipped renut.toml. Copy over culdcept.toml next to
# the NRO, the only toml it reads (renut.toml with CULDCEPT_BUILTIN_CONFIG off). Credits: reNut's renderer (masterspike52,
# reuse permitted), renut_nx_ultimate_base and the ports it carries, renut-nx, sa2-nx, NetherSX2_nx, battd_nx,
# bejeweled3-nx and the zuma ports. Game data is never part of this package.
"""

# Keys set for Culdcept Saga (the rest as the base ships them). Values decided from this port's own logs are filled in by
# the session (docs/culdcept-notes.md).
SHIPPED = {
    'banjo_renderer': '"renut"',
    'renut_gpu_backend': '"deko3d"',
    'banjo_renut_render_height_handheld': '720',
    'banjo_renut_render_height_docked': '1080',
    'banjo_renut_dynamic_resolution': 'false',
    'msaa_level': '1',
    'frame_cap': '"Off"',
    'target_refreshRate': '-1',
    'masseffect_renderer_native': 'false',
    'gpu_plugin': '""',
    'banjo_forwarder_program_id': '""',
    'banjo_shader_misses': '"sdmc:/switch/culdcept-nx/shader_misses"',
    # No composite before the HUD to run SMAA after: the board and the HUD share one shader pair and one target
    # (docs/culdcept-notes.md, "SMAA"). Off; the picture is the game's own.
    'banjo_renut_smaa': 'false',
    # Decided from this port's own log (out/pc_runs/cg13_1, 89 windows: the story and the first board battle): no draw
    # replayed from a recorded command buffer, the inert draw skip 0 in every window.
    'ultimate_renut_replay_inherit_same': 'false',
    'sasrt_gpu_skip_inert_draws': 'false',
    'banjo_renut_tags_check': '256',
    # The user's decision (2026-11-19): the profiler stays on (the SDK's Switch profiler: FPS, CPU and counters in
    # logs/rex/rex_profile.log, the app's banjo_profiler.log, the GPU profile lines), its stack sampler off: it pauses
    # every game thread each 1 ms, and once in Ryujinx (emu21, 1 run of about 20) the app stopped in the profiler's
    # start with it on (docs/culdcept-notes.md, "The package's own settings in Ryujinx"). configs/renut_profile.toml
    # keeps it on for a console profile.
    'switch_profile_stacks': 'false',
}
DROP = re.compile(r'^\s*(katamari_\w+|banjo_cpu_native_sub_8[0-9A-Fa-f]{7})\s*=.*$', re.M)
PLAY = {'log_level': '"off"', 'banjo_renut_diag_resolve_trace': 'false', 'banjo_renut_diag_gpu_shaders': 'false',
        'banjo_renut_diag_terrain': 'false', 'banjo_show_profiler': 'false', 'banjo_show_fps': 'false',
        'banjo_cpu_profiler': 'false', 'switch_profile_stacks': 'false'}
# Culdcept Saga draws its frames with direct calls (no recorded command buffer: the recorder is not linked, 0 replays in
# the PC logs): the fast draw (25), pipelines by content (28), whole-buffer copies (22), the setters' touch compare (26),
# mesh objects (42); 1 (a Vulkan-only key) is the do-nothing control on the Switch. 6 targets x 60 s = 6 minutes a round.
AB = {'banjo_renut_ab_targets': '"25,28,22,26,42,1"', 'banjo_renut_ab_s': '30', 'banjo_renut_dynamic_resolution': 'false'}
VERIFY = {k: 'true' for k in (
    'banjo_renut_fast_draw_verify', 'banjo_renut_resource_objects_verify', 'banjo_renut_fetch_cache_verify',
    'banjo_renut_whole_buffers_verify', 'banjo_renut_persistent_shared_verify', 'banjo_renut_chain_reuse_verify',
    'banjo_renut_snapshot_deltas_verify', 'banjo_renut_store_cards_verify', 'banjo_renut_record_extents_verify')}
VERIFY.update({'banjo_hot_guard_calls': '4000', 'banjo_hot_guard_period': '64', 'banjo_cpu_verify': 'true'})
PGO = {'log_level': '"off"', 'switch_profile_stacks': 'false', 'banjo_cpu_profiler': 'false',
       'banjo_show_profiler': 'false', 'banjo_show_fps': 'false', 'banjo_stall_watch_seconds': '0',
       'renut_gpu_profile': '0.0', 'banjo_renut_diag_resolve_trace': 'false', 'banjo_renut_diag_gpu_shaders': 'false',
       'banjo_renut_diag_terrain': 'false', 'banjo_hot_guard_calls': '0', 'banjo_hot_guard_period': '0'}
# A console CPU profile (docs/optimising-functions.md, "Getting a console profile"): the sampler on, its stacks for the
# hand track's gate.
PROFILE = {'banjo_cpu_profiler': 'true', 'switch_profile_stacks': 'true', 'log_level': '"info"'}
CONFIGS = {
    'renut_verify.toml': (VERIFY, 'every Renut verify mode on (emulator / test runs: slow; the log\'s verify lines '
                                  'must count 0 differences)'),
    'renut_pgo.toml': (PGO, 'the shipped settings with only the crash logs (a PGO build\'s toml: no log, no '
                            'profilers, no diagnostics)'),
    'renut_profile.toml': (PROFILE, 'the shipped settings with the CPU sampler and its stacks on (a console profile '
                                    'for the hot-function method)'),
    'renut_720_docked.toml': ({'banjo_renut_render_height_docked': '720'},
                              '720 lines docked too, the Xbox 360\'s own (pixel-identical)'),
    'renut_msaa2.toml': ({'msaa_level': '2'}, 'MSAA 2x (changes the picture: the user\'s decision)'),
    'renut_dynamic.toml': ({'banjo_renut_dynamic_resolution': 'true'},
                           'dynamic resolution, 540-720 handheld / 720-1080 docked (not pixel-identical)'),
    # culdcept-nx (2026-11-19): in 1 of 3 Ryujinx runs that reached the board, the map's 2D panels (dialogue box, HUD,
    # the village sign) were missing or drawn with another texture (a red tile's) from the map's load on; not seen on
    # the PC or in the other runs (docs/culdcept-notes.md, "Missing 2D panels"). The two texture shortcuts that could
    # keep a stale texture bound, off: the console trial if it shows there.
    'renut_textures_plain.toml': ({'banjo_deko3d_texture_skip': 'false', 'sasrt_renut_texture_memo_fetch_only': 'false'},
                                  'the deko3d texture-bind skip (A/B 39) and the fetch-only texture memo off: a trial if '
                                  'the board\'s 2D panels go missing or show another texture'),
}


def apply(text, keys):
    """Sets each key in place where the base has it at the top level, else before the first [section]."""
    first_section = re.search(r'^\[', text, re.M)
    top_end = first_section.start() if first_section else len(text)
    for k, v in keys.items():
        pat = re.compile(r'^(\s*)' + re.escape(k) + r'\s*=.*$', re.M)
        m = pat.search(text, 0, top_end)
        if m:
            text = text[:m.start()] + f'{m.group(1)}{k} = {v}' + text[m.end():]
        else:
            text = text[:top_end].rstrip('\n') + f'\n{k} = {v}\n\n' + text[top_end:]
        first_section = re.search(r'^\[', text, re.M)
        top_end = first_section.start() if first_section else len(text)
    return text


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', default=os.path.join(here, 'dist', 'switch', 'katamari-nx', 'renut.toml'))
    ap.add_argument('--out', default=os.path.join(here, 'dist', 'switch', 'culdcept-nx'))
    ap.add_argument('--set', action='append', default=[], help='key=value, more shipped settings (the log-decided keys)')
    a = ap.parse_args()
    text = open(a.base, encoding='utf-8').read().replace('\r\n', '\n')
    # The base's own header: the comment lines before its first "# banjo-nx test config" line.
    cut = text.find('# banjo-nx test config')
    if cut > 0:
        text = text[cut:]
    text = DROP.sub('', text).replace('sdmc:/switch/katamari-nx/', 'sdmc:/switch/culdcept-nx/')
    shipped_keys = dict(SHIPPED)
    for kv in a.set:
        k, v = kv.split('=', 1)
        shipped_keys[k.strip()] = v.strip()
    shipped = apply(text, shipped_keys)
    os.makedirs(os.path.join(a.out, 'configs'), exist_ok=True)
    outs = {
        'renut.toml': (shipped, 'the shipped settings with logs on (720 lines handheld, 1080 docked, MSAA off)'),
        'renut_play.toml': (apply(shipped, PLAY), 'renut.toml with the diagnostics and logs off, for playing'),
        'renut_ab.toml': (apply(shipped, AB), 'renut.toml with an in-run A/B list, 30 s halves, target 1 as the '
                                              'control (a Vulkan-only key: nothing on the Switch)'),
    }
    for name, (cfg, what) in CONFIGS.items():
        outs[os.path.join('configs', name)] = (apply(shipped, cfg), what)
    for name, (body, what) in outs.items():
        with open(os.path.join(a.out, name), 'w', encoding='utf-8', newline='\n') as f:
            f.write(HEADER.format(what=what) + body.lstrip('\n'))
        print(name, len(body))
    left = [l for l in shipped.split('\n') if 'atamari' in l]
    print('katamari mentions left in renut.toml:', len(left), left[:5])


if __name__ == '__main__':
    main()
