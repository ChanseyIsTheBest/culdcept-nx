#!/usr/bin/env python3
"""culdcept-nx (2026-11-19): codegen exclusion lists for a set of functions (the run-time HLSL compiler's, here).

Reads a list of function starts (hex, one a line; e.g. the functions reachable by direct calls from
D3DXCompileShaderFromFile, sub_823A0488) and the generated tree's starts, and prints the codegen config's
  non_volatile_as_local_exclude = [[start, end], ...]   (runs of consecutive listed functions merged; end exclusive)
  non_argument_as_local_keep additions                   (the listed starts)
for app/renut_config.toml. A function's end is the next generated function's start.

  python tools/diet_exclude_ranges.py LIST [--generated app/generated] [--min-run 1]
"""
import argparse
import glob
import os
import re


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('list')
    ap.add_argument('--generated', default=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                                                         'app', 'generated'))
    a = ap.parse_args()
    starts = set()
    for p in glob.glob(os.path.join(a.generated, '*_recomp.*.cpp')):
        for m in re.finditer(r'^DEFINE_REX_FUNC\(sub_([0-9A-F]+)\)', open(p, errors='replace').read(), re.M):
            starts.add(int(m.group(1), 16))
    st = sorted(starts)
    nxt = {s: (st[i + 1] if i + 1 < len(st) else s + 4) for i, s in enumerate(st)}
    want = sorted({int(l.strip(), 16) for l in open(a.list) if l.strip()} & starts)
    ranges = []
    for s in want:
        e = nxt[s]
        if ranges and ranges[-1][1] == s:
            ranges[-1][1] = e
        else:
            ranges.append([s, e])
    print(f'# {len(want)} functions in {len(ranges)} ranges')
    print('non_volatile_as_local_exclude = [')
    for s, e in ranges:
        print(f'    [0x{s:08X}, 0x{e:08X}],')
    print(']')
    print('# non_argument_as_local_keep additions:')
    print(', '.join(f'0x{s:08X}' for s in want))


if __name__ == '__main__':
    main()
