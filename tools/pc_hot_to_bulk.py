#!/usr/bin/env python3
"""culdcept-nx (2026-11-19): adds a PC profile's hottest generated functions to the bulk list of machine copies
(docs/optimising-functions.md, "Every opportunity": "Any PC profile ... add what it names to localized_bulk.txt; by
hand"). Reads tools/pc_hot_functions.py's all-threads block, keeps the generated functions (sub_XXXXXXXX) with at least
--min samples, leaves out those the list has and those named with --skip, and appends "<address> <N> PC samples
(<run>)". tools/localize_native.py (run by post_codegen.py) still refuses what it cannot copy.

  python tools/pc_hot_to_bulk.py REPORT --run runA [--min 300] [--skip 82357230] [--list app/src/cpu/hot/localized_bulk.txt]
"""
import argparse
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('report')
    ap.add_argument('--run', required=True)
    ap.add_argument('--min', type=int, default=300)
    ap.add_argument('--skip', action='append', default=[])
    ap.add_argument('--list', default=os.path.join(ROOT, 'app', 'src', 'cpu', 'hot', 'localized_bulk.txt'))
    a = ap.parse_args()
    skip = {int(s, 16) for s in a.skip}
    have = set()
    for line in open(a.list, encoding='utf-8'):
        m = re.match(r'\s*(?:0x)?([0-9A-Fa-f]{8})\b', line)
        if m:
            have.add(int(m.group(1), 16))
    add = []
    for line in open(a.report, encoding='utf-8', errors='replace'):
        if line.startswith('== '):
            break  # the all-threads block ends at the first thread block
        m = re.match(r'\s+[\d.]+%\s+\S*sub_([0-9A-F]{8}) x(\d+)', line)
        if m:
            f, n = int(m.group(1), 16), int(m.group(2))
            if n >= a.min and f not in have and f not in skip:
                add.append((f, n))
    with open(a.list, 'a', encoding='utf-8', newline='\n') as out:
        for f, n in add:
            out.write(f'{f:08X} {n} PC samples ({a.run})\n')
    print(f'{len(add)} functions added to {a.list} ({len(have)} were there)')


if __name__ == '__main__':
    main()
