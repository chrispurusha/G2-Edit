#!/usr/bin/env python3
"""A percussive module's spectral balance and level, the G2 against the engine, hit by hit.

The G2 capture holds several hits (found by their onsets); the engine side is one render per hit, or one render
holding several. Each hit is cut 2 ms before its onset to --window seconds after it, and its energy measured in
bands; the bands are reported relative to the first (the body), so the rig's gain cancels, and the level is
reported as well, corrected by the rig's offset from a calibration patch (--rig, engine minus G2 for a bare OscB
into a 2-Out, measured the same session):

    ./band_balance.py --g2 k1_g2.wav --g2-channel 4 --engine k1_eng*.wav --rig -0.5

Default bands: 20-150 (body), 150-600, 600-2k, 2k-8k, 8k-16k Hz. This is how DrumSynth's Kick presets were
checked (findings 2026-10-11): noise sits in 2-16 kHz, so "too much noise" shows there. Stdlib only.
"""
import argparse, cmath, glob, math, struct, sys, wave


def load(path, channel):
    w = wave.open(path)
    ch, sw, sr = w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(w.getnframes())
    a = struct.unpack('<%d%s' % (len(raw) // sw, {2: 'h', 4: 'i'}[sw]), raw)
    full = 2 ** (8 * sw - 1)
    return [v / full for v in a[channel::ch]], sr


def fft(x):
    n = 1
    while n < len(x):
        n <<= 1
    a = [complex(v) for v in x] + [0j] * (n - len(x))
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    size = 2
    while size <= n:
        w = cmath.exp(-2j * math.pi / size)
        for s in range(0, n, size):
            wk = 1
            for k in range(size // 2):
                u, t = a[s + k], wk * a[s + k + size // 2]
                a[s + k], a[s + k + size // 2] = u + t, u - t
                wk *= w
        size <<= 1
    return a, n


def onsets(x, sr, gap):
    peak = max(abs(v) for v in x)
    found, i = [], 0
    while i < len(x):
        if abs(x[i]) > 0.1 * peak:
            found.append(i)
            i += int(gap * sr)
        else:
            i += 1
    return found


def hit(x, sr, at, window, bands):
    seg = x[max(0, at - int(0.002 * sr)):at + int(window * sr)]
    spec, n = fft(seg)
    energy = [sum(abs(spec[i]) ** 2 for i in range(int(lo * n / sr), int(hi * n / sr))) for lo, hi in bands]
    power = sum(v * v for v in seg) / len(seg)
    return energy, power


def collect(files, channel, window, bands, gap, limit):
    energy, power, count = [0.0] * len(bands), 0.0, 0
    for f in files:
        x, sr = load(f, channel)
        for at in onsets(x, sr, gap)[:limit]:
            e, p = hit(x, sr, at, window, bands)
            energy = [a + b for a, b in zip(energy, e)]
            power += p
            count += 1
    return energy, power / max(count, 1), count


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--g2', required=True); p.add_argument('--engine', required=True, nargs='+')
    p.add_argument('--g2-channel', type=int, default=4); p.add_argument('--engine-channel', type=int, default=0)
    p.add_argument('--window', type=float, default=0.4); p.add_argument('--gap', type=float, default=0.8)
    p.add_argument('--hits', type=int, default=8); p.add_argument('--rig', type=float, default=0.0)
    p.add_argument('--bands', default='20-150,150-600,600-2000,2000-8000,8000-16000')
    args = p.parse_args()
    bands = [tuple(float(v) for v in b.split('-')) for b in args.bands.split(',')]
    eg, pg, ng = collect([args.g2], args.g2_channel, args.window, bands, args.gap, args.hits)
    files = [f for pat in args.engine for f in sorted(glob.glob(pat))]
    ee, pe, ne = collect(files, args.engine_channel, args.window, bands, args.gap, args.hits)
    rel = lambda e: [10 * math.log10(e[i] / e[0]) for i in range(1, len(e))]
    names = ['%g-%g' % b for b in bands[1:]]
    print('bands (dB re %g-%g Hz):  %s' % (bands[0][0], bands[0][1], '  '.join('%10s' % n for n in names)))
    print('G2      (%d hits)         %s' % (ng, '  '.join('%10.1f' % v for v in rel(eg))))
    print('engine  (%d hits)         %s' % (ne, '  '.join('%10.1f' % v for v in rel(ee))))
    print('engine - G2               %s' % '  '.join('%+10.1f' % (a - b) for a, b in zip(rel(ee), rel(eg))))
    print('level: engine - G2 %+.1f dB raw, %+.1f dB after the rig\'s %+.1f' %
          (10 * math.log10(pe / pg), 10 * math.log10(pe / pg) - args.rig, args.rig))


if __name__ == '__main__':
    sys.exit(main())
