#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from vivapinata-nx's tools/d3d_propagate.py, itself bangaio-nx's from nights-nx's): pairs
Viva Pinata's functions (XDK 2.0.3529, D3D9LTCG; vivapinata-nx's generated tree and image dump) with Culdcept Saga's
(XDK 2.0.3424, D3D9LTCG), for moving Renut's Direct3D hooks Katamari -> Viva Pinata (vivapinata-nx's hook lines, which
name Katamari's address in their comment) -> Culdcept Saga.

  seeds   functions whose shape (opcodes and registers, no immediates) is unique in both images and equal; and the
          render / sampler state tables ({getter, setter, default} rows) paired row by row where the defaults agree
          (Viva 0x826F4C18 x97 / 0x826F50A8 x20, Culdcept 0x828B7500 x97 / 0x828B7990 x20: tools/renut_state_tables.py);
  calls   for a paired function whose token sequences align (difflib over the tokens), each aligned `bl` pairs its two
          targets; repeated to a fixpoint. A target paired two different ways is a conflict and dropped.

  python -I tools/d3d_pair_culdcept.py [--viva "D:/viva pinata/vivapinata-nx"]
Writes out/renut_map/viva_to_culdcept.json ({viva: {culdcept, how}}).
"""
import argparse
import difflib
import glob
import hashlib
import json
import os
import re
import struct
from collections import Counter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x82000000
MAX_INSNS = 3000
REF_TABLES = (0x826F4C18, 0x826F50A8)
TGT_TABLES = (0x828B7500, 0x828B7990)
ROWS = (97, 20)


def starts_of(gen):
    s = set()
    for path in glob.glob(os.path.join(gen, '*_recomp.*.cpp')):
        for m in re.finditer(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)', open(path, errors='replace').read(), re.M):
            s.add(int(m.group(1), 16))
    return sorted(s)


def code_range(gen):
    for name in ('renut_config.h', 'renut_init.h'):
        p = os.path.join(gen, name)
        if os.path.exists(p):
            t = open(p, errors='replace').read()
            b = re.search(r'REX_CODE_BASE\s+0x([0-9A-Fa-f]+)', t)
            s = re.search(r'REX_CODE_SIZE\s+0x([0-9A-Fa-f]+)', t)
            if b and s:
                return int(b.group(1), 16), int(b.group(1), 16) + int(s.group(1), 16)
    return 0x82000000, 0x83000000


def token(x):
    op = x >> 26
    rd, ra, rb = (x >> 21) & 31, (x >> 16) & 31, (x >> 11) & 31
    if op in (16, 18):
        return (op, x & 1, rd if op == 16 else 0, ra if op == 16 else 0)
    if op in (19, 31, 63):
        return (op, (x >> 1) & 0x3FF, rd, ra, rb)
    if op == 59:
        return (op, (x >> 1) & 0x1F, rd, ra, rb)
    if op == 30:
        return (op, (x >> 2) & 7, rd, ra)
    if op == 4:
        return (op, x & 0x7FF, rd)
    return (op, rd, ra)


class Img:
    def __init__(self, path, gen):
        self.b = open(path, 'rb').read()
        lo, hi = code_range(gen)
        self.starts = [s for s in starts_of(gen) if lo <= s < hi]
        self.f = {}
        for i, s in enumerate(self.starts):
            e = self.starts[i + 1] if i + 1 < len(self.starts) else s + 256
            n = min((e - s) // 4, MAX_INSNS)
            words = [struct.unpack_from('>I', self.b, s - BASE + 4 * k)[0] for k in range(n)]
            self.f[s] = (words, [token(w) for w in words])
        self.hash = {s: hashlib.sha1(repr(t).encode()).hexdigest() for s, (w, t) in self.f.items()}

    def calls(self, s):
        out = []
        for k, w in enumerate(self.f[s][0]):
            if w >> 26 == 18 and w & 1:
                d = w & 0x03FFFFFC
                if d & 0x02000000:
                    d -= 0x04000000
                out.append((k, (s + 4 * k + d) & 0xFFFFFFFF))
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--viva', default='D:/viva pinata/vivapinata-nx')
    a = ap.parse_args()
    P = Img(os.path.join(a.viva, 'out', 'renut_image.bin'), os.path.join(a.viva, 'app', 'generated'))
    N = Img(os.path.join(ROOT, 'out', 'renut_image.bin'), os.path.join(ROOT, 'app', 'generated'))
    pairs, how = {}, {}
    ph, nh = Counter(P.hash.values()), Counter(N.hash.values())
    nby = {h: s for s, h in N.hash.items()}
    for s, h in P.hash.items():
        if ph[h] == 1 and nh.get(h) == 1 and len(P.f[s][0]) >= 6:
            pairs[s] = nby[h]
            how[s] = 'unique shape'
    for pa, na, rows in zip(REF_TABLES, TGT_TABLES, ROWS):
        for i in range(rows):
            pr = struct.unpack_from('>3I', P.b, pa - BASE + 12 * i)
            nr = struct.unpack_from('>3I', N.b, na - BASE + 12 * i)
            if pr[2] != nr[2]:
                continue
            for c in range(2):
                if pr[c] and nr[c]:
                    pairs[pr[c]] = nr[c]
                    how[pr[c]] = f'state table row {i} col {c}'
    conflicts = set()
    done = set()
    changed = True
    while changed:
        changed = False
        for p in list(pairs):
            n = pairs[p]
            if p in done or p not in P.f or n not in N.f:
                continue
            done.add(p)
            pt, nt = P.f[p][1], N.f[n][1]
            pc, nc = dict(P.calls(p)), dict(N.calls(n))
            if pt == nt:
                idx = [(k, k) for k in pc]
            else:
                sm = difflib.SequenceMatcher(None, pt, nt, autojunk=False)
                if sm.ratio() < 0.6:
                    continue
                idx = []
                for tag, i1, i2, j1, j2 in sm.get_opcodes():
                    if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1):
                        idx += [(i1 + k, j1 + k) for k in range(i2 - i1) if i1 + k in pc]
            for i, j in idx:
                if j not in nc:
                    continue
                tp, tn = pc[i], nc[j]
                if tp in conflicts:
                    continue
                if tp in pairs:
                    if pairs[tp] != tn:
                        conflicts.add(tp)
                    continue
                pairs[tp] = tn
                how[tp] = f'call at +{4 * i:#x} of {p:08X}'
                changed = True
    for c in conflicts:
        if how.get(c, '').startswith('call'):
            pairs.pop(c, None)
    out = os.path.join(ROOT, 'out', 'renut_map')
    os.makedirs(out, exist_ok=True)
    json.dump({f'{k:08X}': {'culdcept': f'{v:08X}', 'how': how[k]} for k, v in sorted(pairs.items())},
              open(os.path.join(out, 'viva_to_culdcept.json'), 'w'), indent=1)
    print('pairs', len(pairs), 'conflicts', len(conflicts), Counter(h.split(' ')[0] for h in how.values()))


if __name__ == '__main__':
    main()
