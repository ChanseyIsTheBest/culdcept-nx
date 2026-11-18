#!/usr/bin/env python3
"""culdcept-nx (2026-11-19, from vivapinata-nx's tools/d3d_data_addrs_viva.py): the guest data addresses Renut's Direct3D
layer reads, at Culdcept Saga's: constant blocks by content (Viva Pinata's bytes, word for word; the build qword by its
value), variables by the instruction that builds them in the paired function (out/renut_map/viva_to_culdcept.json).

  python -I tools/d3d_data_addrs_culdcept.py
"""
import json
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = 0x82000000
V = open(r'D:\viva pinata\vivapinata-nx\out\renut_image.bin', 'rb').read()
P = open(os.path.join(ROOT, 'out', 'renut_image.bin'), 'rb').read()
pairs = {k: v['culdcept'] for k, v in json.load(open(os.path.join(ROOT, 'out', 'renut_map', 'viva_to_culdcept.json'))).items()}


def find_all(data, needle, align=4):
    out, i = [], data.find(needle)
    while i >= 0:
        if i % align == 0:
            out.append(i + B)
        i = data.find(needle, i + 1)
    return out


def refs(img, fn, n=600):
    """(index, address, opcode) of every lis + D-form / addi use in fn (to its first blr after 8 instructions)."""
    hi, out = {}, []
    for k in range(n):
        w = struct.unpack_from('>I', img, fn - B + 4 * k)[0]
        op, rd, ra, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        s = imm - 0x10000 if imm & 0x8000 else imm
        if op == 15 and ra == 0:
            hi[rd] = (imm << 16) & 0xFFFFFFFF
        elif ra in hi and op in (14, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 44, 45, 48, 50, 52, 54, 58, 62):
            out.append((k, (hi[ra] + s) & 0xFFFFFFFF, op))
        if w == 0x4E800020 and k > 8:
            break
    return out


print('content searches (Viva -> Culdcept)')
for name, addr, size in (('format table', 0x820B5698, 128), ('format table, first 64', 0x820B5698, 64)):
    print(f'  {name}: Viva {addr:08X} -> {[hex(x) for x in find_all(P, V[addr - B:addr - B + size])]}')
for build in (3424, 3529):
    q = struct.pack('>HHHH', 2, 0, build, 0x4000)
    print(f'  the build qword 2.0.{build}.0 (0x4000):', [hex(x) for x in find_all(P, q, 8)])
    q2 = struct.pack('>HHH', 2, 0, build)
    print(f'  2.0.{build} any:', [hex(x) for x in find_all(P, q2, 8)])
print('variables through the paired functions (Viva -> Culdcept)')
for name, vfn, vaddr in (('VdGlobalDevice record (D3D_LockResource)', 0x8245CC78, 0x82000824),
                         ('HiZ owner (CreateSurface)', 0x8245C670, 0x82A26CEC),
                         ('HiZ owner (Destroy)', 0x8245CA78, 0x82A26CEC),
                         ('no-large-pages flag (XPhysicalAlloc)', 0x826B34B8, 0x827466DC),
                         ('build qword (device init)', 0x8246BB88, 0x82722DF0)):
    k = f'{vfn:08X}'
    pfn = pairs.get(k)
    vrefs = refs(V, vfn)
    print(f'  {name}: Viva fn {k} refs {[hex(a) for _, a, _ in vrefs][:10]}; Culdcept fn {pfn}')
    if pfn:
        prefs = refs(P, int(pfn, 16))
        print(f'     Culdcept refs {[hex(a) for _, a, _ in prefs][:10]}')
        idx = [i for i, (_, a, _) in enumerate(vrefs) if a == vaddr]
        for i in idx:
            if i < len(prefs):
                print(f'     Viva ref #{i} {vaddr:08X} -> Culdcept {prefs[i][1]:08X}')
for name, vfn in (('XMemFree (Viva overrides it)', 0x821574C8), ('XMemAlloc (Viva overrides it)', 0x82157408),
                  ('GetCurrentThreadId', 0x826B8718), ('XPhysicalAlloc', 0x826B34B8), ('device Release', 0x82457720)):
    print(f'  {name}: Viva {vfn:08X} -> {pairs.get(f"{vfn:08X}")}')
