#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from vivapinata-nx's tools/renut_hook_map_viva.py): where each of Renut's Direct3D hooks
lives in Culdcept Saga (XDK 2.0.3424, D3D9LTCG).

The base's hook lines (app/src/renut_engine/gpu/**.cpp) stand at Beautiful Katamari's addresses (XDK 2.0.5632), or at
N&B's for the ones Katamari could not place (BANJO_RENUT_UNHOOKED_*). vivapinata-nx (XDK 2.0.3529, the nearest XDK of
our ports) moved the same lines to Viva Pinata's addresses and names Katamari's / N&B's in each comment. Route:

  base line (Katamari or N&B address, handler)  ->  Viva's line for the same handler (or naming that address)
  ->  Viva's address  ->  out/renut_map/viva_to_culdcept.json (tools/d3d_pair_culdcept.py)  ->  Culdcept's address

tools/renut_hook_map_culdcept.txt (`<base sub> <Culdcept address or -> <evidence>`) wins over the route.

  python -I tools/renut_hook_map_culdcept.py [--viva "D:/viva pinata/vivapinata-nx"]
Writes out/renut_map/hook_map.json and prints the coverage.
"""
import argparse
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LINE = re.compile(r'^\s*BANJO_RENUT_(UNHOOKED_)?HOOK(_RAW)?(_SHARED)?\(\s*sub_([0-9A-F]{8})\s*(?:,\s*([A-Za-z_]\w*))?')
NOTE = re.compile(r'(Katamari|N&B) sub_([0-9A-F]{8})')


def lines_of(tree):
    out = []
    for p in glob.glob(os.path.join(tree, 'app', 'src', 'renut_engine', 'gpu', '**', '*.cpp'), recursive=True):
        for i, l in enumerate(open(p, encoding='utf-8', errors='replace')):
            m = LINE.match(l)
            if m:
                notes = dict((k, v) for k, v in NOTE.findall(l))
                out.append(dict(file=os.path.relpath(p, tree).replace('\\', '/'), line=i + 1, unhooked=bool(m.group(1)),
                                raw=bool(m.group(2)), shared=bool(m.group(3)), addr=m.group(4), handler=m.group(5),
                                kat=notes.get('Katamari'), nb=notes.get('N&B'), text=l.strip()))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--viva', default='D:/viva pinata/vivapinata-nx')
    a = ap.parse_args()
    base = lines_of(ROOT)
    viva = lines_of(a.viva)
    pairs = {k: v['culdcept'] for k, v in json.load(open(os.path.join(ROOT, 'out', 'renut_map', 'viva_to_culdcept.json'))).items()}
    hand = {}
    hp = os.path.join(ROOT, 'tools', 'renut_hook_map_culdcept.txt')
    if os.path.exists(hp):
        for l in open(hp, encoding='utf-8'):
            l = l.split('#')[0].strip()
            if l:
                k, v = l.split()[:2]
                hand[k.replace('sub_', '').upper()] = v
    by_kat, by_nb, by_handler = {}, {}, {}
    for v in viva:
        if v['kat']:
            by_kat.setdefault(v['kat'], []).append(v)
        if v['nb']:
            by_nb.setdefault(v['nb'], []).append(v)
        if v['handler']:
            by_handler.setdefault((v['handler'], v['raw']), []).append(v)
    # Viva's hooked functions the pairing left out: the best field signature in Culdcept's Direct3D library
    # (tools/renut_sigmatch.py's score), at least 0.70 and 0.05 ahead of the next.
    sig = {}
    want = set()
    for b in base:
        for c in by_kat.get(b['addr'], []) + by_nb.get(b['addr'], []):
            if not c['unhooked'] and c['addr'] not in pairs:
                want.add(c['addr'])
    if want:
        import sys
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        import renut_sigmatch as sm
        vt = sm.load_tree(os.path.join(a.viva, 'app', 'generated'))
        ct = sm.load_tree(os.path.join(ROOT, 'app', 'generated'))
        lo, hi = 0x82170000, 0x821D0000
        pool = {k: v for k, v in ct.items() if lo <= int(k[4:], 16) < hi}
        for v in sorted(want):
            fv = vt.get('sub_' + v)
            if not fv:
                print(f'  sigmatch Viva {v}: not in its tree')
                continue
            sc = sorted(((sm.score(fv, fc), k) for k, fc in pool.items()), reverse=True)[:3]
            if sc and sc[0][0] >= 0.70 and (len(sc) < 2 or sc[0][0] - sc[1][0] >= 0.05):
                sig[v] = (sc[0][1][4:], f"Viva {v} by signature {sc[0][0]:.2f} (next {sc[1][0]:.2f} {sc[1][1]})")
            print(f'  sigmatch Viva {v}: ' + ', '.join(f'{k} {x:.2f}' for x, k in sc))
    result = {}
    stats = {}
    for b in base:
        key = f"{b['file']}:{b['line']}:{b['addr']}"
        cands = by_kat.get(b['addr'], []) + by_nb.get(b['addr'], [])
        if not cands and b['handler']:
            same = by_handler.get((b['handler'], b['raw']), [])
            if len(same) == 1:  # a generic handler (the state tables' Set/GetRenderStateHook) matches only by its note
                cands = same
        if b['addr'] in hand:
            tgt, why = hand[b['addr']], 'hand list'
        elif not cands:
            tgt, why = None, 'no Viva line'
        else:
            hooked = [c for c in cands if not c['unhooked']]
            if not hooked:
                tgt, why = None, 'unhooked in Viva'
            else:
                vs = sorted(set(c['addr'] for c in hooked))
                ts = sorted(set(pairs.get(v) for v in vs if pairs.get(v)))
                if len(ts) == 1:
                    tgt, why = ts[0], f"Viva {','.join(vs)}"
                elif not ts and len(vs) == 1 and vs[0] in sig:
                    tgt, why = sig[vs[0]]
                elif not ts:
                    tgt, why = None, f"Viva {','.join(vs)} not paired"
                else:
                    tgt, why = None, f"Viva {','.join(vs)} -> {ts}: conflict"
        if tgt == '-':
            tgt = None
        stats[why.split(' ')[0] if not why.startswith('Viva') or tgt else why.split(' ', 2)[-1].split(':')[-1].strip()] = 0
        result[key] = dict(base=b['addr'], handler=b['handler'], raw=b['raw'], unhooked_in_base=b['unhooked'],
                           target=tgt, why=why, text=b['text'][:160])
    json.dump(result, open(os.path.join(ROOT, 'out', 'renut_map', 'hook_map.json'), 'w'), indent=1)
    placed = sum(1 for r in result.values() if r['target'])
    print(f'{len(base)} hook lines in the base, {len(viva)} in Viva; placed {placed}')
    from collections import Counter
    c = Counter()
    for r in result.values():
        if r['target']:
            c['placed'] += 1
        else:
            c[r['why'] if not r['why'].startswith('Viva') else ('Viva not paired' if 'not paired' in r['why'] else 'conflict')] += 1
    print(dict(c))
    for k, r in sorted(result.items()):
        if not r['target'] and r['why'] not in ('unhooked in Viva',):
            print(f"  {r['why']:40s} {k} {r['handler']}")


if __name__ == '__main__':
    main()
