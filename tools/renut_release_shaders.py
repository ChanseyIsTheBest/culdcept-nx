#!/usr/bin/env python3
"""katamari-nx (2026-11-02, futari-nx's and peggle2-nx's tools/renut_release_shaders.py): the Switch build's Renut shader
files without translated game shaders, for a build that can be distributed (app/out/renut_switch_release/), and the
file the web installer makes in their place, for tests.

    python tools/renut_release_shaders.py [--glsl out/deko3d_shaders/glsl_opt] [--cache app/out/renut_switch/shader_cache_switch.cpp]

Writes into app/out/renut_switch_release/:
  shader_cache_switch.cpp   the shader table (numbers: hashes, fetch masks, vertex input formats) without the SPIR-V
                            (the deko3d back end never reads it; the PC's Vulkan one does)
  sd/deko3d_glsl.bin        the package of the game's shaders, which the web installer makes from the player's files and
                            puts beside the NRO (this one is the Python tools' copy: game data, for tests only, never
                            distributed)
Every one of Beautiful Katamari's 195 shaders is a container stored in its files (111 in default.xex, 84 in the models
of d02-d05.dat), so no GLSL package is built into the NRO: app/cmake/banjo_renut_gpu.cmake
(BANJO_RENUT_RELEASE_SHADERS) builds with these files.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def package(glsl_dir, names, out_dir, raw=False):
    """tools/deko3d_shader_package.py over the given shaders only (raw: no dictionary, which zstd cannot train on a
    handful of shaders)."""
    os.makedirs(out_dir, exist_ok=True)
    src = os.path.join(out_dir, 'glsl')
    shutil.rmtree(src, ignore_errors=True)
    os.makedirs(src)
    for n in names:
        shutil.copyfile(os.path.join(glsl_dir, n), os.path.join(src, n))
    subprocess.run([sys.executable, '-P', os.path.join(ROOT, 'tools', 'deko3d_shader_package.py'), '--glsl', src,
                    '--out', out_dir] + (['--raw'] if raw else []), check=True)
    shutil.rmtree(src)


def strip_spirv(text):
    """shader_cache_switch.cpp without the SPIR-V: the assembler's .incbin goes, its sizes are 0, and every entry's
    SPIR-V offset and size (the 4th and 5th numbers of g_shaderCacheEntries' lines) are 0."""
    text, n = re.subn(r'// banjo-nx: the compressed SPIR-V.*?\.previous\\n"\);\n',
                      'const uint8_t g_compressedSpirvCache[] = {0};  // no SPIR-V (deko3d reads none)\n',
                      text, flags=re.S)
    if n != 1:
        sys.exit('renut_release_shaders: the SPIR-V block of shader_cache_switch.cpp not found')
    text = re.sub(r'(g_spirvCache(?:Compressed|Decompressed)Size\s*=\s*)\d+', r'\g<1>0', text)

    def entry(m):
        fields = m.group(2).split(', ')
        fields[3] = fields[4] = '0'
        return m.group(1) + ', '.join(fields)
    text, n = re.subn(r'^(\t\{ )(0x[0-9A-F]+, \d+, \d+, \d+, \d+)', entry, text, flags=re.M)
    if not n:
        sys.exit('renut_release_shaders: no entry of g_shaderCacheEntries found')
    if 'incbin' in text:
        sys.exit('renut_release_shaders: an .incbin is left in the shader table')
    return text


def runtime_shaders(dump, image_path, out_dir):
    """culdcept-nx (2026-11-21): the shaders of the dump that are not in default.xex's image (the ones the game makes at
    run time: Culdcept Saga compiles two from data/shader.fx and builds two from parts), copied into out_dir as captures
    for --only. The page makes the others from the player's default.xex (culdcept-nx-installer's release manifest lists
    them by SHA-256). Returns (runtime names, image names), as <vs|ps>_<HASH>.glsl."""
    image = open(image_path, 'rb').read()
    shutil.rmtree(out_dir, ignore_errors=True)
    os.makedirs(out_dir)
    runtime, in_image = [], []
    for n in sorted(os.listdir(dump)):
        if not re.fullmatch(r'(vs|ps)_[0-9a-fA-F]{16}\.bin', n):
            continue
        blob = open(os.path.join(dump, n), 'rb').read()
        glsl = n[:2] + '_' + n[3:19].upper() + '.glsl'
        if image.find(blob) >= 0:
            in_image.append(glsl)
        else:
            runtime.append(glsl)
            shutil.copyfile(os.path.join(dump, n), os.path.join(out_dir, n))
    return runtime, in_image


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--glsl', default=os.path.join(ROOT, 'out', 'deko3d_shaders', 'glsl_opt'))
    ap.add_argument('--cache', default=os.path.join(ROOT, 'app', 'out', 'renut_switch', 'shader_cache_switch.cpp'))
    ap.add_argument('--out', default=os.path.join(ROOT, 'app', 'out', 'renut_switch_release'))
    # culdcept-nx (2026-11-21, banjo-nx's distribution build): the shaders the game makes at run time stay built in
    # (their GLSL, raw frames, and their entries of the table alone); the page makes the rest from default.xex.
    ap.add_argument('--keep-runtime', action='store_true',
                    help='build in the shaders of --dump that are not in --image (the game makes them at run time)')
    ap.add_argument('--dump', default=os.path.join(ROOT, 'out', 'renut_shader_dump'))
    ap.add_argument('--image', default=os.path.join(ROOT, 'out', 'renut_image.bin'))
    a = ap.parse_args()
    names = sorted(f for f in os.listdir(a.glsl) if re.fullmatch(r'(vs|ps)_[0-9A-F]{16}\.glsl', f))
    if not names:
        sys.exit(f'renut_release_shaders: no shaders in {a.glsl}')
    os.makedirs(a.out, exist_ok=True)
    if a.keep_runtime:
        sys.path.insert(0, os.path.join(ROOT, 'tools'))
        from renut_switch_shader_cache import only_table
        captures = os.path.join(a.out, 'runtime_captures')
        runtime, in_image = runtime_shaders(a.dump, a.image, captures)
        if sorted(runtime + in_image) != names:
            sys.exit('renut_release_shaders: the dump and the GLSL folder name different shaders')
        package(a.glsl, in_image, os.path.join(a.out, 'sd'), raw=True)
        os.remove(os.path.join(a.out, 'sd', 'deko3d_glsl_switch.cpp'))  # the installer's file is never built in
        subprocess.run([sys.executable, '-P', os.path.join(ROOT, 'tools', 'deko3d_shader_package.py'), '--glsl', a.glsl,
                        '--out', a.out, '--only', captures, '--raw'], check=True)
        only_table(open(a.cache, encoding='utf-8').read(), captures, a.out)
        print(f'renut_release_shaders: built in {len(runtime)} shaders the game makes at run time '
              f'({", ".join(runtime)}); beside the NRO {len(in_image)} from default.xex -> {a.out}')
        return
    package(a.glsl, names, os.path.join(a.out, 'sd'))
    os.remove(os.path.join(a.out, 'sd', 'deko3d_glsl_switch.cpp'))  # the installer's file is never built in
    text = open(a.cache, encoding='utf-8').read()
    with open(os.path.join(a.out, 'shader_cache_switch.cpp'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(strip_spirv(text))
    print(f'renut_release_shaders: no shader built in, beside the NRO {len(names)} game shaders -> {a.out}')


if __name__ == '__main__':
    main()
