#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from vivapinata-nx's tools/renut_offset_pairs_viva.py): Direct3D device offsets, Viva Pinata's
layout (XDK 2.0.3529) -> Culdcept Saga's (XDK 2.0.3424).

Aligns the opcodes of every paired Direct3D function (out/renut_map/viva_to_culdcept.json, tools/d3d_pair_culdcept.py, and
the signature matches of out/renut_map/hook_map.json) with difflib, and for each aligned D-form load / store through a
register other than r1 gives one (Viva, Culdcept) displacement pair. Each Viva displacement is reported with how often
each answer came, so a stray pair shows as a minority against the field's real move.

  python -I tools/renut_offset_pairs_culdcept.py [--min 0x100] [--out out/renut_map/offset_pairs.txt]
"""
import argparse
import difflib
import json
import os
import re
import sys
from collections import Counter, defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from d3d_pair_culdcept import Img, ROOT  # noqa: E402

VIVA = 'D:/viva pinata/vivapinata-nx'
VIVA_D3D = (0x82457000, 0x82474000)
CUL_D3D = (0x82170000, 0x821D0000)
DFORM = set(range(32, 56))
DS = {58, 62}


def disp(w):
    op = w >> 26
    if op in DFORM:
        d = w & 0xFFFF
    elif op in DS:
        d = w & 0xFFFC
    else:
        return None
    d = d - 0x10000 if d & 0x8000 else d
    if (w >> 16) & 31 == 1:
        return None
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--min', type=lambda x: int(x, 0), default=0x100)
    ap.add_argument('--out', default=os.path.join(ROOT, 'out', 'renut_map', 'offset_pairs.txt'))
    a = ap.parse_args()
    V = Img(os.path.join(VIVA, 'out', 'renut_image.bin'), os.path.join(VIVA, 'app', 'generated'))
    C = Img(os.path.join(ROOT, 'out', 'renut_image.bin'), os.path.join(ROOT, 'app', 'generated'))
    pairs = {}
    for k, v in json.load(open(os.path.join(ROOT, 'out', 'renut_map', 'viva_to_culdcept.json'))).items():
        va, ca = int(k, 16), int(v['culdcept'], 16)
        if VIVA_D3D[0] <= va < VIVA_D3D[1] and CUL_D3D[0] <= ca < CUL_D3D[1]:
            pairs[va] = ca
    hm = os.path.join(ROOT, 'out', 'renut_map', 'hook_map.json')
    if os.path.exists(hm):
        for r in json.load(open(hm)).values():
            m = re.match(r'Viva ([0-9A-F]{8}) by signature', r['why'] or '')
            if m and r['target']:
                pairs.setdefault(int(m.group(1), 16), int(r['target'], 16))
    table = defaultdict(Counter)
    where = {}
    used = 0
    same = 0
    for va, ca in pairs.items():
        if va not in V.f or ca not in C.f:
            continue
        vw, cw = V.f[va][0], C.f[ca][0]
        sm = difflib.SequenceMatcher(None, [w >> 26 for w in vw], [w >> 26 for w in cw], autojunk=False)
        if sm.ratio() < 0.6:
            continue
        used += 1
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag != 'equal':
                continue
            for k in range(i2 - i1):
                x, y = vw[i1 + k], cw[j1 + k]
                dx, dy = disp(x), disp(y)
                if dx is None or dy is None or dx < a.min or dy < 0:
                    continue
                table[dx][dy] += 1
                same += dx == dy
                where.setdefault((dx, dy), f'{va:08X}/{ca:08X}')
    moved = []
    with open(a.out, 'w', encoding='utf-8') as out:
        for dx in sorted(table):
            parts = []
            for dy, n in table[dx].most_common(4):
                parts.append(f'{dy}({dy - dx:+d})x{n} [{where[(dx, dy)]}]')
            top = table[dx].most_common(1)[0][0]
            if top != dx:
                moved.append((dx, top, table[dx][top], sum(table[dx].values())))
            out.write(f'{dx} 0x{dx:X}: ' + ', '.join(parts) + '\n')
    total = sum(sum(c.values()) for c in table.values())
    print(f'{used} of {len(pairs)} function pairs aligned; {len(table)} Viva offsets, {total} accesses, {same} the same')
    print(f'{len(moved)} Viva offsets whose commonest Culdcept answer differs:')
    for dx, dy, n, t in moved:
        print(f'  {dx} (0x{dx:X}) -> {dy} ({dy - dx:+d}) {n} of {t}')


if __name__ == '__main__':
    main()
