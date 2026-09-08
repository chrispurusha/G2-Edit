"""Harmonic magnitudes from a capture of a steady tone.

   HOW TO MEASURE A WAVEFORM'S SPECTRUM, and how NOT to. Written after three wrong answers.

   DO NOT read harmonics off a period-synchronously averaged cycle. The period comes from an
   INTEGER sample lag, so it is quantised; over the hundreds of cycles you average to get a clean
   shape, the accumulated phase error smears the upper harmonics away. The result looks EXACTLY
   like a capture-chain lowpass - it showed an apparent 25 dB roll-off by the sixth harmonic on a
   path that is in fact flat - and it shifts and rounds the peak, so any measurement of waveform
   SHAPE (a breakpoint, a duty, a peak position) is corrupted too. That artefact produced two
   complete sets of plausible, wrong wave constants before it was spotted.

   Instead: refine f0 by maximising the sum of the first few harmonics, then read each harmonic
   with a Hann-windowed DFT at exactly k*f0. That is what this does.

   ALWAYS VALIDATE BEFORE TRUSTING A NUMBER. Capture OscB's own sawtooth and square through the
   same path first: the saw must come back at -6.0/-9.5/-12.0/-14.0 dB and the square must show NO
   even harmonics. This module returns them to 0.2 dB. If it does not, fix that before fitting
   anything.

   TWO MORE TRAPS.
   - Check the instrument's output level. A capture taken with the volume down puts everything
     above the second harmonic into the noise floor and yields a confident, wrong fit.
   - PIN f0 when the fundamental can vanish. DblSaw at full Shape is two antiphase saws with no
     first harmonic at all, and Sine3/Sine4 go the same way; the detector then locks onto the
     second harmonic and every number after it is nonsense. Pass f0 explicitly there.

   The rig: build OscShpB -> 2-Out with the backdoor (ADDMODULE, CABLE, PUSH), then DEVMODE the
   waveform and DEVSET parameter 6 for Shape. The instrument free-runs, so no note is needed.
   Capture with tools/capture; the G2 arrives on QU-24 input 5, which is channel 4 here."""
import sys, math

# READS THROUGH analyse_ir, which is the repository's own WAV reader. This used to import a module
# called `wav` from a scratchpad directory belonging to a session that ended months ago, so the tool
# has been dead on arrival - "No module named 'wav'" - ever since, and every harmonic analysis since
# then was written out from scratch instead of using it. analyse_ir.read_wav() is better anyway: it
# builds ONLY the channel asked for, which matters when a 192 kHz eight-channel take would otherwise
# want gigabytes to answer a question about one of them.
from analyse_ir import read_wav


def load(path, ch):
    """`ch` is ZERO-BASED, as the docstring above uses it - the G2 on QU-24 input 5 is channel 4.
       read_wav() numbers from one, hence the +1."""
    channels, rate = read_wav(path, wanted=[ch + 1])

    return rate, channels[ch]

def goertzel(x, rate, freq, win):
    n = len(x); w = 2.0 * math.pi * freq / rate
    re = sum(x[i] * win[i] * math.cos(w * i) for i in range(n))
    im = sum(x[i] * win[i] * math.sin(w * i) for i in range(n))
    return 2.0 * math.hypot(re, im) / (sum(win) or 1.0)

def analyse(path, ch, harmonics=8, seconds=0.5, coarse=None, pin=None):
    rate, xs = load(path, ch)
    start = int(0.6 * rate); n = int(seconds * rate)
    x = xs[start:start + n]
    win = [0.5 - 0.5 * math.cos(2 * math.pi * i / (n - 1)) for i in range(n)]
    if coarse is None:                       # coarse pitch by autocorrelation
        m = min(len(x), rate // 2); mean = sum(x) / len(x)
        y = [v - mean for v in x]
        best, bl = -1e30, 100
        for lag in range(int(rate / 1200), int(rate / 60)):
            s = sum(y[i] * y[i + lag] for i in range(0, m - lag, 8))
            if s > best: best, bl = s, lag
        coarse = rate / bl
    best = (-1.0, coarse)                    # refine on the harmonic sum
    f = coarse * 0.995
    while f <= coarse * 1.005:
        tot = sum(goertzel(x, rate, f * k, win) for k in (1, 2, 3, 4))
        if tot > best[0]: best = (tot, f)
        f += coarse * 0.00002
    f0 = pin if pin is not None else best[1]

    # SUB-OCTAVE GUARD. The coarse autocorrelation locks onto twice the period readily - a saw
    # correlates with itself just as well at 2T as at T - and every harmonic after that is read at
    # the wrong place: on a synthetic 220 Hz saw it reported 110 Hz and then +157 dB for the second
    # "harmonic", which is the real fundamental seen from an empty bin. If the candidate's own
    # fundamental is far weaker than the bin an octave up, the octave up is the real one.
    if pin is None:
        for _ in range(2):
            a = goertzel(x, rate, f0, win)
            b = goertzel(x, rate, f0 * 2.0, win)

            if b > (a * 8.0):        # 18 dB, far more than any real harmonic series inverts by
                f0 *= 2.0
            else:
                break
    h = [goertzel(x, rate, f0 * k, win) for k in range(1, harmonics + 1)]
    return f0, [20 * math.log10(max(v, 1e-12) / h[0]) for v in h]

if __name__ == "__main__":
    f0, db = analyse(sys.argv[1], int(sys.argv[2]))
    print("f0 %.3f Hz   " % f0 + " ".join("%6.1f" % v for v in db[1:]))
