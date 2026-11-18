#!/usr/bin/env python3
"""culdcept-nx (2026-11-19): the fixes vivapinata-nx made after its remap for XDK 2.0.3529's device (its commits 24da6e4f and
4dfb802f), which hold for Culdcept Saga's XDK 2.0.3424 (the same device layout: tools/renut_offset_pairs_culdcept.py).
Run after tools/renut_port_culdcept.py and tools/renut_device_offsets_culdcept.py. Idempotent.

1. The device's viewport holds integers in this XDK (SetViewport stores X, Y and the clamped width and height with stw,
   MinZ and MaxZ with stfs); Katamari's XDK keeps six floats. Stored as floats, game code reading the width as an integer
   got 1280.0f's bits (vivapinata-nx: every 2D panel shrank to nothing). Katamari's seventh word (+24) is this XDK's
   ScissorRect: not written.
2. Offsets the zone tools miss: GetRenderState's bits(0x...) fields and the wrap setter's two words, -128 (the packets).

  python -I tools/renut_xdk3424_fixes.py [--check]
"""
import argparse
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GPU = os.path.join(ROOT, 'app', 'src', 'renut_engine', 'gpu')
M = '// culdcept-nx (vivapinata-nx 24da6e4f): integers in this XDK'
EDITS = [
    ('d3d/d3d_native.h', '''void SetViewport(Device device, u32 viewport_va);  // D3DVIEWPORT9*
''', '''void SetViewport(Device device, u32 viewport_va);  // D3DVIEWPORT9*
// culdcept-nx (vivapinata-nx 24da6e4f): the device's viewport as SetViewport takes it: x, y, width, height are integers in
// XDK 2.0.3424 / 3529, the z range floats.
inline void GetViewportValues(Device device, float out[6]) {
  for (u32 i = 0; i < 4; ++i)
    out[i] = float(i32(device.W(0x3058 + i * 4)));
  out[4] = device.F(0x3068);
  out[5] = device.F(0x306C);
}
'''),
    ('d3d/d3d_native_state.cpp', '''  i32 x1 = TruncateToI32(device.F(kViewport));
  i32 y1 = TruncateToI32(device.F(kViewport + 4));
  const i32 width = TruncateToI32(device.F(kViewport + 8));
  const i32 height = TruncateToI32(device.F(kViewport + 12));''', f'''  {M} (SetViewport below).
  i32 x1 = i32(device.W(kViewport));
  i32 y1 = i32(device.W(kViewport + 4));
  const i32 width = i32(device.W(kViewport + 8));
  const i32 height = i32(device.W(kViewport + 12));'''),
    ('d3d/d3d_native_state.cpp', '''  device.SetF(kViewport, x);
  device.SetF(kViewport + 4, y);
  device.SetF(kViewport + 16, min_z);
  device.SetF(kViewport + 20, max_z);
  device.SetF(kViewport + 12, fh);
  device.SetW(kViewport + 24, 0);
  device.SetF(kViewport + 8, fw);''', f'''  {M}: four integers and two floats, as the game's SetViewport stores
  // them; Katamari's seventh word (+24) is this XDK's ScissorRect: not written.
  (void)fw;
  (void)fh;
  device.SetW(kViewport, u32(ix));
  device.SetW(kViewport + 4, u32(iy));
  device.SetF(kViewport + 16, min_z);
  device.SetF(kViewport + 20, max_z);
  device.SetW(kViewport + 12, u32(clamped_height));
  device.SetW(kViewport + 8, u32(clamped_width));'''),
    ('d3d/d3d_native_state.cpp', '''  const i32 vx = TruncateToI32(device.F(kViewport));
  const i32 vy = TruncateToI32(device.F(kViewport + 4));
  const i32 vw = TruncateToI32(device.F(kViewport + 8));
  const i32 vh = TruncateToI32(device.F(kViewport + 12));''', f'''  const i32 vx = i32(device.W(kViewport));  {M}
  const i32 vy = i32(device.W(kViewport + 4));
  const i32 vw = i32(device.W(kViewport + 8));
  const i32 vh = i32(device.W(kViewport + 12));'''),
    ('d3d/hooks/state.cpp', '''viewport[i] = u32(native::TruncateToI64(d.F(0x3058 + i * 4)));''',
     '''viewport[i] = d.W(0x3058 + i * 4);  // culdcept-nx (vivapinata-nx 24da6e4f): integers in this XDK'''),
    ('render/draw.cpp', '''    viewport[i] = device.F32(kClearViewportOffset + i * 4);''',
     '''    viewport[i] = float(int32_t(device.U32(kClearViewportOffset + i * 4)));  // culdcept-nx (vivapinata-nx 24da6e4f): integers in this XDK'''),
    # 2. GetRenderState's bits() and the wrap setter (vivapinata-nx 4dfb802f), -128.
    ('d3d/d3d_native_state.cpp', 'constexpr u32 kWord = Index < 8 ? 0x292C : 0x2930;',
     'constexpr u32 kWord = Index < 8 ? 0x28AC : 0x28B0;  // culdcept-nx (vivapinata-nx 4dfb802f): 0x292C->0x28AC, 0x2930->0x28B0, missed by the zone tools'),
    ('d3d/d3d_native_state.cpp', 'case 57: return bits(0x2978, 0, 2);              // TessellationMode',
     'case 57: return bits(0x28F8, 0, 2);              // TessellationMode culdcept-nx (vivapinata-nx 4dfb802f): 0x2978->0x28F8'),
    ('d3d/d3d_native_state.cpp', 'case 76: return bits(0x294C, 0, 1);              // ViewportEnable',
     'case 76: return bits(0x28CC, 0, 1);              // ViewportEnable culdcept-nx: 0x294C->0x28CC'),
    ('d3d/d3d_native_state.cpp', 'case 81: return bits(0x29C0, 0, 1);              // HalfPixelOffset',
     'case 81: return bits(0x2940, 0, 1);              // HalfPixelOffset culdcept-nx: 0x29C0->0x2940'),
    ('d3d/d3d_native_state.cpp', 'case 98: return bits(0x29B8, 10, 1);             // LastPixel',
     'case 98: return bits(0x2938, 10, 1);             // LastPixel culdcept-nx: 0x29B8->0x2938'),
    ('d3d/d3d_native_state.cpp', 'return bits(0x292C + (wrap / 8) * 4, (wrap % 8) * 4, 4);',
     'return bits(0x28AC + (wrap / 8) * 4, (wrap % 8) * 4, 4);  // culdcept-nx: 0x292C->0x28AC'),
]
LOOP = 'for (u32 i = 0; i < 6; ++i)\n      viewport[i] = d.F(0x3058 + i * 4);'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()
    done = skipped = 0
    bad = []
    for rel, old, new in EDITS:
        p = os.path.join(GPU, rel.replace('/', os.sep))
        t = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
        crlf = '\r\n' in t
        o, n = (old.replace('\n', '\r\n'), new.replace('\n', '\r\n')) if crlf else (old, new)
        if n in t:
            skipped += 1
        elif o in t:
            t = t.replace(o, n)
            done += 1
            if not a.check:
                open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(t)
        else:
            bad.append((rel, old[:80]))
    # BeginTiling and Present read the six viewport words into floats: GetViewportValues.
    p = os.path.join(GPU, 'd3d', 'hooks', 'draw.cpp')
    t = open(p, encoding='utf-8', errors='surrogateescape', newline='').read()
    nl = '\r\n' if '\r\n' in t else '\n'
    loops = 0
    out = []
    lines = t.split(nl)
    i = 0
    while i < len(lines):
        if lines[i].strip() == 'for (u32 i = 0; i < 6; ++i)' and i + 1 < len(lines) and 'viewport[i] = d.F(0x3058 + i * 4);' in lines[i + 1]:
            indent = lines[i][:len(lines[i]) - len(lines[i].lstrip())]
            out.append(indent + 'native::GetViewportValues(d, viewport);  // culdcept-nx (vivapinata-nx 24da6e4f): integers in this XDK')
            i += 2
            loops += 1
            continue
        out.append(lines[i])
        i += 1
    if loops and not a.check:
        open(p, 'w', encoding='utf-8', errors='surrogateescape', newline='').write(nl.join(out))
    print(f'{done} edits applied, {skipped} already there, {len(bad)} missing; {loops} viewport loops in hooks/draw.cpp')
    for b in bad:
        print('MISSING', b)
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
