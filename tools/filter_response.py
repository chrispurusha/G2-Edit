#!/usr/bin/env python3
"""A filter's response, the G2 against the engine, per third-octave band.

The rig puts the filtered signal on one channel and the same signal dry on the other (Noise > filter > 2-Out L,
Noise > 2-Out R), so the response is L/R and the rig's gain cancels. Give it pairs of files - a G2 capture and an
engine render of the same setting:

    ./filter_response.py --g2 g2.wav --g2-channels 4,5 --engine eng.wav [--engine-channels 0,1] [--fft 16384]

Several pairs: --pairs DIR, reading DIR/<tag>_g2.wav and DIR/<tag>_eng.wav. Prints, per setting, the bands
compared (those within --floor dB of the response's peak), the worst and mean |engine - G2|, and where the worst is.

KEEP THE FFT LONG (default 16384). At 4096 points a bin is 23 Hz at 96 kHz and 12 Hz at 48 kHz, so the low
third-octave bands hold one or two bins laid out differently on the two sides - that showed 2-4 dB that were
not there (validation-sessions.md, traps). Stdlib only.
"""
import argparse, cmath, glob, math, os, struct, sys, wave


def load(path, channels):
    w = wave.open(path)
    ch, sw, sr = w.getnchannels(), w.getsampwidth(), w.getframerate()
    raw = w.readframes(w.getnframes())
    a = struct.unpack('<%d%s' % (len(raw) // sw, {2: 'h', 4: 'i'}[sw]), raw)
    full = 2 ** (8 * sw - 1)
    return [[v / full for v in a[c::ch]] for c in channels], sr


class Fft:
    def __init__(self, n):
        self.n = n
        bits = n.bit_length() - 1
        self.rev = [int(format(i, '0%db' % bits)[::-1], 2) for i in range(n)]
        self.tw = [cmath.exp(-2j * math.pi * k / n) for k in range(n // 2)]
        self.win = [0.5 - 0.5 * math.cos(2 * math.pi * i / n) for i in range(n)]

    def power(self, x):
        n = self.n
        a = [complex(x[self.rev[i]] * self.win[self.rev[i]]) for i in range(n)]
        size = 2
        while size <= n:
            half, step = size // 2, n // size
            for s in range(0, n, size):
                for k in range(half):
                    t = self.tw[k * step] * a[s + k + half]
                    u = a[s + k]
                    a[s + k] = u + t
                    a[s + k + half] = u - t
            size <<= 1
        return [abs(v) ** 2 for v in a[:n // 2]]

    def psd(self, x):
        total = [0.0] * (self.n // 2)
        for s in range(0, len(x) - self.n, self.n // 2):
            for i, p in enumerate(self.power(x[s:s + self.n])):
                total[i] += p
        return total


CENTRES = [63 * 2 ** (k / 3) for k in range(25) if 63 * 2 ** (k / 3) <= 16500]


def response(path, channels, fft, start, end):
    (filtered, dry), sr = load(path, channels)
    a, b = int(start * sr), int(end * sr)
    pf, pd = fft.psd(filtered[a:b]), fft.psd(dry[a:b])
    out = []
    for c in CENTRES:
        lo, hi = c / 2 ** (1 / 6), c * 2 ** (1 / 6)
        i0 = max(1, int(lo * fft.n / sr))
        i1 = max(i0 + 1, int(hi * fft.n / sr))
        out.append(10 * math.log10(sum(pf[i0:i1]) / max(sum(pd[i0:i1]), 1e-30) + 1e-30))
    return out


def compare(g2, eng, args, fft, tag):
    rg = response(g2, args.g2_channels, fft, args.start, args.end)
    re_ = response(eng, args.engine_channels, fft, args.start, args.end)
    top = max(rg)
    idx = [i for i in range(len(CENTRES)) if rg[i] > top - args.floor]
    diffs = [(re_[i] - rg[i], CENTRES[i]) for i in idx]
    worst = max(diffs, key=lambda t: abs(t[0]))
    mean = sum(abs(t[0]) for t in diffs) / len(diffs)
    print(f"{tag:24s} {len(idx):3d}   {worst[0]:+6.2f}   {mean:5.2f}   {worst[1]:7.0f} Hz")
    return abs(worst[0])


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--g2'); p.add_argument('--engine'); p.add_argument('--pairs')
    p.add_argument('--g2-channels', default='4,5'); p.add_argument('--engine-channels', default='0,1')
    p.add_argument('--fft', type=int, default=16384); p.add_argument('--floor', type=float, default=40.0)
    p.add_argument('--start', type=float, default=0.5); p.add_argument('--end', type=float, default=2.9)
    args = p.parse_args()
    args.g2_channels = [int(c) for c in args.g2_channels.split(',')]
    args.engine_channels = [int(c) for c in args.engine_channels.split(',')]
    fft = Fft(args.fft)
    print("setting                  bands  worst    mean    at")
    worst = []
    if args.pairs:
        for g in sorted(glob.glob(os.path.join(args.pairs, '*_g2.wav'))):
            tag = os.path.basename(g)[:-len('_g2.wav')]
            worst.append(compare(g, os.path.join(args.pairs, tag + '_eng.wav'), args, fft, tag))
    elif args.g2 and args.engine:
        worst.append(compare(args.g2, args.engine, args, fft, os.path.basename(args.g2)))
    else:
        p.error('--g2 and --engine, or --pairs')
    print("overall worst %.2f dB" % max(worst))


if __name__ == '__main__':
    sys.exit(main())
