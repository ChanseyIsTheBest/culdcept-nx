#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, vivapinata-nx's tools/renut_device_offsets_culdcept.py as it is): Renut's Direct3D device offsets,
from Beautiful Katamari's layout (XDK 2.0.5632) to Culdcept Saga's (XDK 2.0.3424), which is Viva Pinata's (XDK 2.0.3529)
field for field: tools/renut_offset_pairs_culdcept.py over 414 paired functions, 1,556 of 1,658 accesses the same and
every other one a global constant (lis + displacement), not a device field; the device allocated 0x5080 and cleared
0x5000 by Direct3D_CreateDevice sub_82173000 as Viva's sub_82457638 does. The zones are vivapinata-nx's.

vivapinata-nx's text:

Every literal katamari-nx moved carries its N&B (title update 3) value in the comment: `katamari-nx: X->Y`, Y being Katamari's. The new value
comes from Y by the zones below, measured with tools/renut_offset_pairs_viva.py over the paired functions (out/renut_map/offset_pairs.txt:
Katamari's displacement, this game's, how many aligned accesses agree). Literals equal to Y on the line are rewritten, and the line's comment says
`vivapinata-nx: Y->new`. Fields below 0x2B00 written as struct members go through d3d.h's Device, moved by hand with its static asserts.

  python -I tools/renut_device_offsets_culdcept.py [--dry-run]
"""
import argparse
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')
PAIR = re.compile(r'katamari-nx: ?((?:0x[0-9A-Fa-f]+|\d+)->(?:0x[0-9A-Fa-f]+|\d+)(?:, ?(?:0x[0-9A-Fa-f]+|\d+)->(?:0x[0-9A-Fa-f]+|\d+))*)')
ONE = re.compile(r'(0x[0-9A-Fa-f]+|\d+)->(0x[0-9A-Fa-f]+|\d+)')

# Katamari -> Viva PiÃ±ata. (first, last, delta); accesses in out/renut_map/offset_pairs.txt.
ZONES = [
    (0x480, 10931, -128),   # the constants (0x480 -> 0x400) and packets, 369 accesses to 10368-10916
    (10932, 11003, -132),   # flags and fences, 81 accesses to 10932-11000
    (11004, 11839, -272),   # 11816-11824, 3 accesses (XenonGpu_Initialize `std.11816 -> std.11544`)
    (11840, 12660, -264),   # the render-state block, 178 accesses to 11840-12520; the viewport 12640.. -> 12376.. (sub_8245A0F8)
    (12661, 13169, -268),   # index buffer ... predication, 68 accesses to 12668-12740, 13164
    (13170, 13330, -272),   # tiling clear values and fields, 19 accesses to 13180-13240, BeginTiling's stw.13172 -> 12900
    (13331, 13799, -352),   # command buffer ... present interval, 56 accesses to 13332-13584
    (14000, 16520, -1396),  # auto surfaces, ring, critical sections, 61 accesses to 14800-14916; the gamma ramp 14980 -> 13584 (SetGammaRamp sub_8245AB78, 1536 bytes)
    (16521, 21528, -1552),  # side buffer and swap fields (16704, 21508-21528), 13 accesses
    (21529, 21559, -1548),  # the device init's -1 words 21532, 21536 -> 19984, 19988 (sub_8246BB88), 21544 -> 19996
    (21560, 21700, -1544),  # frequency and period 21560, 21564 -> 20016, 20020; 21568-21640, 18 accesses
    (21701, 22259, -1852),  # the gamma words 22244, 22248 -> 20392, 20396 (Swap's lbz.20400 of 22248's low byte), 22260 -> 20408
    (22260, 22263, -1852),
    (22264, 22300, -1848),  # behaviour flags 22264 -> 20416 (`stw r30,20416(r31)` in Direct3D_CreateDevice)
]
EXPLICIT = {0x5700: 0x5000,  # kDeviceAllocSize: Direct3D_CreateDevice sub_82457638 `li r5,20480` clears it (allocates 20608)
            0x2E24: 0x2D10,  # the declaration: SetVertexDeclaration sub_8245E0B8 `stw r4,11536(r3)`, 4 bytes more than its neighbours
            0x31A0: 0x3098,  # kDeviceSnapshotSize, a size, not a field
            0x4098: 0x4E0C}  # SwapCount: Swap sub_8246ABF8 lwz/stw 19980


def culdcept(y):
    if y in EXPLICIT:
        return EXPLICIT[y]
    for lo, hi, d in ZONES:
        if lo <= y <= hi:
            return y + d
    return None


def respell(old, new):
    return f'0x{new:X}' if old.lower().startswith('0x') else str(new)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    changed_lines = unplaced = 0
    report = []
    for d, _, fs in os.walk(GPU):
        for f in fs:
            if not f.endswith(('.cpp', '.h', '.inc')):
                continue
            p = os.path.join(d, f)
            t = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
            lines = t.split('\n')
            dirty = False
            for i, line in enumerate(lines):
                if 'culdcept-nx:' in line:
                    continue
                m = PAIR.search(line)
                if not m:
                    continue
                code, sep, comment = line.partition('//')
                notes = []
                for xs, ys in ONE.findall(m.group(1)):
                    y = int(ys, 0)
                    if y >= 0x10000:  # a guest data address: tools/renut_port_culdcept.py
                        continue
                    new = culdcept(y)
                    if new is None:
                        unplaced += 1
                        report.append(f'NO ZONE {f}:{i + 1} Katamari {y}: {line.strip()[:90]}')
                        continue
                    rx = re.compile(r'(?<![0-9A-Za-z_.])' + re.escape(ys) + r'(?:[uU])?(?![0-9A-Za-z_.])')
                    if rx.search(code):
                        code = rx.sub(respell(ys, new), code, count=1)
                        notes.append(f'{ys}->{respell(ys, new)}')
                    else:
                        report.append(f'NOT IN CODE {f}:{i + 1} {ys}: {code.strip()[:90]}')
                if notes:
                    lines[i] = code + sep + comment.rstrip('\r') + ' (culdcept-nx: ' + ', '.join(notes) + ')' + ('\r' if line.endswith('\r') else '')
                    dirty = True
                    changed_lines += 1
            if dirty and not a.dry_run:
                open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write('\n'.join(lines))
    print(f'{changed_lines} lines rewritten, {unplaced} values with no zone')
    print('\n'.join(report))


if __name__ == '__main__':
    main()
