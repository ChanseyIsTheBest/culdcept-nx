"""culdcept-nx (2026-11-20): an Opus re-encode of an XMA sound against the XMA decode it was made from, as the port would
play them: NAME.ref.wav (the frames decoded in order by FFmpeg's xmaframes decoder, 512 samples each) and NAME.opus
(the same samples in Opus), both written by culdcept-nx-installer's test/audio_node.mjs --wav. The port takes frame k's
samples from [512 k, 512 k + 512) of the Opus stream after its pre-skip, so the two must line up with no offset:
prints the offset that matches best (0 expected), the SNR at offset 0 and the worst second's.
  python tools/xmo_compare.py <folder or NAME.ref.wav> [...]   (FFMPEG: ffmpeg, default C:/ffmpeg/ffmpeg.exe)
"""
import os
import subprocess
import sys
import wave

import numpy as np

FFMPEG = os.environ.get('FFMPEG', 'C:/ffmpeg/ffmpeg.exe')


def load_wav(path):
    with wave.open(path, 'rb') as w:
        ch = w.getnchannels()
        data = np.frombuffer(w.readframes(w.getnframes()), dtype='<i2').astype(np.float64) / 32767
    return data.reshape(-1, ch), ch


def load_opus(path, ch):
    r = subprocess.run([FFMPEG, '-hide_banner', '-loglevel', 'error', '-i', path, '-f', 'f32le', '-ac', str(ch), '-ar',
                        '48000', '-'], capture_output=True, check=True)
    return np.frombuffer(r.stdout, dtype='<f4').astype(np.float64).reshape(-1, ch)


def snr(ref, test):
    noise = np.sum((ref - test) ** 2)
    sig = np.sum(ref ** 2)
    return float('inf') if noise == 0 else 10 * np.log10(max(sig, 1e-20) / noise)


def compare(ref_path):
    opus_path = ref_path[:-len('.ref.wav')] + '.opus'
    ref, ch = load_wav(ref_path)
    dec = load_opus(opus_path, ch)
    n = min(len(ref), len(dec))
    # the best offset within +-2048 samples, on the loudest 2 s
    m = ref[:n].mean(axis=1)
    win = min(n, 96000)
    energy = np.convolve(m ** 2, np.ones(4800), 'valid')
    start = int(np.argmax(energy[:max(1, n - win)])) if n > win else 0
    seg = m[start:start + win]
    best, best_lag = -1e9, 0
    d = dec[:, 0] if ch == 1 else dec.mean(axis=1)
    for lag in range(-2048, 2049, 1):
        a, b = start + lag, start + lag + win
        if a < 0 or b > len(d):
            continue
        c = float(np.dot(seg, d[a:b]))
        if c > best:
            best, best_lag = c, lag
    total = snr(ref[:n], dec[:n])
    worst = min((snr(ref[i:i + 48000], dec[i:i + 48000]) for i in range(0, n - 48000, 48000)
                 if np.sum(ref[i:i + 48000] ** 2) > 1e-3 * 48000 * ch * 1e-3), default=total)
    return dict(name=os.path.basename(ref_path)[:-len('.ref.wav')], seconds=n / 48000, ch=ch, lag=best_lag, snr=total,
                worst=worst, length_diff=len(dec) - len(ref))


def main():
    paths = []
    for a in sys.argv[1:]:
        if os.path.isdir(a):
            for root, _, files in os.walk(a):
                paths += [os.path.join(root, f) for f in files if f.endswith('.ref.wav')]
        else:
            paths.append(a)
    for p in sorted(paths):
        r = compare(p)
        print(f"{r['name']:22} {r['seconds']:6.1f} s {r['ch']} ch: best offset {r['lag']:+5d} samples, SNR {r['snr']:5.1f} dB "
              f"(worst second {r['worst']:5.1f} dB), Opus {r['length_diff']:+d} samples")


if __name__ == '__main__':
    main()
