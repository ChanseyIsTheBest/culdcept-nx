#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from vivapinata-nx's tools/renut_hook_remap_viva.py): moves Renut's hook lines
(app/src/renut_engine/gpu/**.cpp, at Beautiful Katamari's addresses, or N&B's for the unhooked ones) to Culdcept Saga's,
from out/renut_map/hook_map.json (tools/renut_hook_map_culdcept.py).

A placed hook becomes BANJO_RENUT_HOOK...(sub_<Culdcept>, ...) with `// culdcept-nx: Katamari sub_K; <route>`. One with
no place becomes BANJO_RENUT_UNHOOKED_...(unlinked_<K>, ...): the handler compiles, nothing is registered, and no other
game's address stays in the code ("Stale mentions" in docs/optimising-functions.md). Two lines may not take one
address: the second is left unhooked and reported. Idempotent (lines with `culdcept-nx:` are skipped).

  python -I tools/renut_hook_remap_culdcept.py [--dry-run]
"""
import argparse
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')
LINE = re.compile(r'^(\s*)BANJO_RENUT_(UNHOOKED_)?HOOK(_RAW)?(_SHARED)?\(\s*sub_([0-9A-F]{8})(.*)$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    hm = json.load(open(os.path.join(ROOT, 'out', 'renut_map', 'hook_map.json')))
    used, placed, off, dup = {}, 0, 0, []
    for path in sorted(glob.glob(os.path.join(GPU, '**', '*.cpp'), recursive=True)):
        rel = os.path.relpath(path, ROOT).replace('\\', '/')
        text = open(path, encoding='utf-8', newline='').read()
        nl = '\r\n' if '\r\n' in text else '\n'
        lines = text.split(nl)
        changed = False
        for k, line in enumerate(lines):
            m = LINE.match(line)
            if not m or 'culdcept-nx:' in line:
                continue
            indent, was_off, raw, shared, addr, rest = m.groups()
            v = hm.get(f'{rel}:{k + 1}:{addr}')
            if v is None:
                raise SystemExit(f'{rel}:{k + 1}: not in the hook map (run tools/renut_hook_map_culdcept.py on this tree)')
            target = v['target']
            if target and target in used:
                dup.append((rel, k + 1, addr, target, used[target]))
                target = None
            suffix = f'{raw or ""}{shared or ""}'
            code, sep, comment = rest.partition('//')
            note = f'// culdcept-nx: {"N&B" if was_off else "Katamari"} sub_{addr}'
            why = (v['why'] or '')[:110]
            if target:
                used[target] = addr
                lines[k] = f'{indent}BANJO_RENUT_HOOK{suffix}(sub_{target}{code.rstrip()}  {note}; {why}'
                placed += 1
            else:
                lines[k] = f'{indent}BANJO_RENUT_UNHOOKED_HOOK{suffix}(unlinked_{addr}{code.rstrip()}  {note}; not placed in this game: {why}'
                off += 1
            changed = True
        if changed and not a.dry_run:
            open(path, 'w', encoding='utf-8', newline='').write(nl.join(lines))
    print(f'{placed} hooks placed, {off} unhooked, {len(dup)} duplicates')
    for d in dup:
        print('DUPLICATE', d)


if __name__ == '__main__':
    main()
