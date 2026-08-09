#!/usr/bin/env python3
"""Turn a measure.py capture into numbers: stage lengths, decay times, spectra.

THE CAPTURE IT EXPECTS
    Four channels from one take: 1-2 the DRY impulse straight from a spare output pair, 3-4 the module
    under test. The dry pair earns its place three times over — it locates every impulse to the sample
    without guessing from the processed signal, it IS the system reference (converters, cabling and
    interface) captured in the same take so deconvolution cannot drift, and comparing dry repeats to
    each other says whether the excitation is repeatable at all.

    Two channels also work (wet only): onsets are then found in the wet signal, which is less exact.

WHY IT REFUSES TO AVERAGE SOMETIMES
    Averaging repeats is how the noise floor comes down, and it is only valid if the repeats are
    sample-aligned AND phase-identical. A note-gated excitation need not start at the same phase every
    time. Averaging repeats that differ in phase does not add noise — it CANCELS signal, quietly, and
    leaves a plausible-looking result that is wrong. So consistency is measured first and reported, and
    below a threshold this refuses to average and analyses repeats separately instead.

Stdlib only, deliberately: this has to run wherever the editor builds. numpy would make it faster and
is worth installing if these captures get long, but nothing here needs it.
"""

import argparse, math, struct, sys, wave


# ---------- reading ----------

def read_wav(path):
    with wave.open(path, "rb") as w:
        ch, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    full = 1 << (8 * width - 1)
    out = [[0.0] * frames for _ in range(ch)]
    step = width * ch
    for i in range(frames):
        base = i * step
        for c in range(ch):
            o = base + c * width
            v = int.from_bytes(raw[o:o + width], "little", signed=True)
            out[c][i] = v / full
    return out, rate


# ---------- onsets ----------

def find_onsets(x, rate, min_gap_s=0.25, rel_thresh=0.35):
    """Impulse positions: the first sample of each burst above a fraction of the loudest peak."""
    peak = max(abs(v) for v in x) or 1.0
    thresh, gap, onsets, i, n = peak * rel_thresh, int(rate * min_gap_s), [], 0, len(x)
    while i < n:
        if abs(x[i]) >= thresh:
            onsets.append(i)
            i += gap
        else:
            i += 1
    return onsets


# ---------- consistency ----------

def correlation(a, b):
    n = min(len(a), len(b))
    sa = sum(a[i] * a[i] for i in range(n))
    sb = sum(b[i] * b[i] for i in range(n))
    if sa <= 0 or sb <= 0:
        return 0.0
    return sum(a[i] * b[i] for i in range(n)) / math.sqrt(sa * sb)


def align_to_peak(ir, search=4000):
    """Shift an impulse response so its largest early sample sits at index 0.

    NECESSARY BEFORE COMPARING OR AVERAGING REPEATS. Each response's time origin comes from its own
    detected onset, and onset detection jitters by a few samples when repeat levels vary — which they do
    when the excitation is a note whose phase is not repeatable. A few samples of shift barely changes
    how a response LOOKS and destroys any correlation between two of them, so repeats appear
    inconsistent when in truth they are merely offset. Aligning on the response's own peak gives them
    the common origin that comparison assumes.
    """
    n = min(len(ir), search)
    if n == 0:
        return ir
    pk = max(range(n), key=lambda i: abs(ir[i]))
    return ir[pk:] + [0.0] * pk


def consistency(segs, window):
    """Lowest correlation of any repeat against the first, over the opening `window` samples."""
    if len(segs) < 2:
        return 1.0
    ref = segs[0][:window]
    return min(correlation(ref, s[:window]) for s in segs[1:])


# ---------- measurements ----------

def early_peaks(ir, rate, span_s=0.05, rel=0.06):
    """Local maxima in the opening of the IR: for a delay tank these are the stage lengths."""
    n = min(len(ir), int(rate * span_s))
    env = [abs(v) for v in ir[:n]]
    peak = max(env) or 1.0
    out = []
    for i in range(n):
        # SAMPLE 0 IS A PEAK IF IT RISES TO THE RIGHT, and it is the one that matters most: the direct
        # impulse sits there and every stage time is measured relative to it. Starting the loop at 1
        # dropped it silently, which the self-test caught.
        left  = env[i - 1] if i > 0 else -1.0
        right = env[i + 1] if i + 1 < n else -1.0
        if env[i] >= left and env[i] > right and env[i] >= peak * rel:
            out.append((i, env[i] / peak))
    return out


def rt60(ir, rate):
    """Schroeder backward integration, fitted between -5 and -35 dB and extrapolated to -60."""
    tail = [v * v for v in ir]
    total, acc, edc = sum(tail), 0.0, [0.0] * len(tail)
    if total <= 0:
        return None
    for i in range(len(tail) - 1, -1, -1):
        acc += tail[i]
        edc[i] = acc
    db = [10.0 * math.log10(max(e / total, 1e-20)) for e in edc]
    def crossing(level):
        for i, d in enumerate(db):
            if d <= level:
                return i
        return None
    a, b = crossing(-5.0), crossing(-35.0)
    if a is None or b is None or b <= a:
        return None
    return ((b - a) / rate) * (60.0 / 30.0)


def fft(re, im):
    n = len(re)
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit; bit >>= 1
        j |= bit
        if i < j:
            re[i], re[j] = re[j], re[i]
            im[i], im[j] = im[j], im[i]
    length = 2
    while length <= n:
        ang = -2.0 * math.pi / length
        wr, wi = math.cos(ang), math.sin(ang)
        for i in range(0, n, length):
            cr, ci = 1.0, 0.0
            for k in range(i, i + length // 2):
                l = k + length // 2
                tr = re[l] * cr - im[l] * ci
                ti = re[l] * ci + im[l] * cr
                re[l], im[l] = re[k] - tr, im[k] - ti
                re[k], im[k] = re[k] + tr, im[k] + ti
                cr, ci = cr * wr - ci * wi, cr * wi + ci * wr
        length <<= 1
    return re, im


def ifft(re, im):
    """Inverse via the conjugate trick, so one forward transform serves both directions."""
    n = len(re)
    im = [-v for v in im]
    re, im = fft(re, im)
    return [v / n for v in re], [-v / n for v in im]


def deconvolve(wet, dry, eps_db=-60.0):
    """Recover the impulse response from ANY excitation, given a simultaneous dry copy of it.

    WHY THIS IS HERE. A one-sample click is the simplest excitation but not the only workable one: an
    envelope-shaped burst is easier on levels and kinder to the converters, and it carries its own
    attack and decay which would otherwise be mistaken for early reflections. Dividing the wet signal
    by the dry one in the frequency domain removes the excitation's shape whatever it was, leaving the
    response of the path between them.

    It also fixes something a Dirac cannot: if the excitation is NOT identical every repeat — a note's
    start phase need not be — each repeat still divides by its own dry copy, so the resulting responses
    are directly comparable and can be averaged even when the raw recordings could not be.

    Regularised rather than a plain divide: where the dry signal has no energy the quotient is
    meaningless and would explode, so the denominator gets a floor at eps_db below its own peak.
    """
    n = 1
    while n < max(len(wet), len(dry)):
        n <<= 1
    wr = list(wet) + [0.0] * (n - len(wet))
    dr = list(dry) + [0.0] * (n - len(dry))
    wr, wi = fft(wr, [0.0] * n)
    dr, di = fft(dr, [0.0] * n)
    mags = [dr[k] * dr[k] + di[k] * di[k] for k in range(n)]
    floor = max(mags) * (10.0 ** (eps_db / 10.0))
    qr, qi = [0.0] * n, [0.0] * n
    for k in range(n):
        den = mags[k] + floor
        qr[k] = (wr[k] * dr[k] + wi[k] * di[k]) / den      # W * conj(D) / (|D|^2 + eps)
        qi[k] = (wi[k] * dr[k] - wr[k] * di[k]) / den
    re, _ = ifft(qr, qi)
    return re[:len(wet)]


def spectrum(ir, rate, size=4096):
    seg = list(ir[:size]) + [0.0] * max(0, size - len(ir))
    for i in range(size):                     # Hann, so the tail does not ring the estimate
        seg[i] *= 0.5 - 0.5 * math.cos(2.0 * math.pi * i / (size - 1))
    re, im = fft(seg, [0.0] * size)
    out = []
    for k in range(1, size // 2):
        mag = math.hypot(re[k], im[k])
        out.append((k * rate / size, 20.0 * math.log10(max(mag, 1e-12))))
    return out


# ---------- calibration ----------

def calibrate(chans, rate, ref=None):
    """Per-channel level and timing, from a take where ONE signal is sent to ALL outputs.

    WHY THIS COMES FIRST. The dry pair is only a valid amplitude reference for the wet pair if the two
    output paths, the two cable runs and the two desk input trims agree. If they do not, every level the
    analysis reports is wrong by that difference and a deconvolution is wrong by it too — silently,
    because nothing about the result looks unusual. Sending one impulse to all four outputs and reading
    the channels back is a two-minute check that removes the doubt.

    It reports timing as well as level: the output pairs should be sample-aligned, and if they are not,
    that offset has to come out before the dry channel can be used as a reference.
    """
    active = []
    for i, x in enumerate(chans):
        peak = max(abs(v) for v in x) if x else 0.0
        if peak > 0.001:                      # anything quieter is an unpatched input, not a signal
            rms = math.sqrt(sum(v * v for v in x) / len(x))
            onsets = find_onsets(x, rate)
            active.append({"ch": i + 1, "peak": peak, "rms": rms, "onsets": onsets})

    if not active:
        print("no channel carries signal — check routing, trims and that the patch is sounding")
        return
    if ref is None:
        ref = active[0]["ch"]
    base = next((a for a in active if a["ch"] == ref), active[0])

    print(f"channels carrying signal: {[a['ch'] for a in active]}   reference: channel {base['ch']}\n")
    print(f"  {'ch':>3}  {'peak dBFS':>9}  {'vs ref':>7}  {'RMS dBFS':>8}  {'impulses':>8}  {'1st onset':>9}  {'vs ref':>7}")
    for a in active:
        d_peak = 20.0 * math.log10(a["peak"] / base["peak"]) if base["peak"] > 0 else 0.0
        first  = a["onsets"][0] if a["onsets"] else None
        bfirst = base["onsets"][0] if base["onsets"] else None
        d_t    = (first - bfirst) if (first is not None and bfirst is not None) else None
        print(f"  {a['ch']:>3}  {20.0*math.log10(a['peak']):>9.2f}  {d_peak:>+7.2f}"
              f"  {20.0*math.log10(max(a['rms'],1e-12)):>8.2f}  {len(a['onsets']):>8}"
              f"  {first if first is not None else '-':>9}  {(('%+d' % d_t) if d_t is not None else '-'):>7}")

    worst = max(abs(20.0 * math.log10(a["peak"] / base["peak"])) for a in active if base["peak"] > 0)
    print()
    if worst < 0.2:
        print(f"  LEVELS MATCH to {worst:.2f} dB — the dry channel can be used as an amplitude reference.")
    elif worst < 1.0:
        print(f"  {worst:.2f} dB spread. Usable, but trim it out on the desk or pass the offset to the")
        print(f"  analysis rather than ignoring it.")
    else:
        print(f"  {worst:.2f} dB SPREAD — too much. Match the desk trims before capturing anything,")
        print(f"  or the dry reference will misstate the wet path by this amount.")
    offs = [a["onsets"][0] - base["onsets"][0] for a in active if a["onsets"] and base["onsets"]]
    if offs and max(abs(o) for o in offs) > 1:
        print(f"  TIMING: channels differ by up to {max(abs(o) for o in offs)} samples "
              f"({max(abs(o) for o in offs)/rate*1000:.3f} ms) — subtract that before using the dry reference.")
    else:
        print("  TIMING: output pairs are sample-aligned.")


# ---------- self test ----------

def selftest():
    rate = 96000
    truth = [(0, 1.0), (411, 0.62), (1103, 0.48), (2677, 0.31)]     # known "stage" positions
    want_rt = 1.4
    ir = [0.0] * (rate * 3)
    for pos, amp in truth:
        ir[pos] += amp
    decay = math.exp(-6.9078 / (want_rt * rate))                    # -60 dB over want_rt
    g, seed = 1.0, 12345
    for i in range(len(ir)):
        seed = (1103515245 * seed + 12345) & 0x7fffffff
        ir[i] = ir[i] * 1.0 + g * ((seed / 0x7fffffff) - 0.5) * 0.02
        g *= decay
    peaks = early_peaks(ir, rate)
    found = [p for p, _ in peaks]
    ok_peaks = all(any(abs(f - t) <= 2 for f in found) for t, _ in truth)
    got_rt = rt60(ir, rate)
    ok_rt = got_rt is not None and abs(got_rt - want_rt) / want_rt < 0.25
    segs = [ir[:8192], ir[:8192], [-v for v in ir[:8192]]]
    ok_cons = consistency(segs[:2], 4096) > 0.99 and consistency(segs, 4096) < 0.0
    print(f"  early peaks : {'PASS' if ok_peaks else 'FAIL'}  (wanted {[t for t,_ in truth]}, found {found[:6]})")
    print(f"  RT60        : {'PASS' if ok_rt else 'FAIL'}  (wanted {want_rt}s, got {got_rt and round(got_rt,3)}s)")
    print(f"  consistency : {'PASS' if ok_cons else 'FAIL'}  (identical ~1.0, inverted negative)")
    sp = spectrum(ir, rate)
    ok_sp = len(sp) == 2047 and all(math.isfinite(d) for _, d in sp)
    print(f"  spectrum    : {'PASS' if ok_sp else 'FAIL'}  ({len(sp)} bins)")

    # DECONVOLUTION, against the case that actually matters: a decaying burst rather than a click.
    # Build a known short IR, excite it with an envelope-shaped burst, then check the peaks come back.
    size  = 8192
    short = [0.0] * size
    for pos, amp in [(0, 1.0), (137, 0.7), (409, 0.45), (908, 0.3)]:
        short[pos] = amp
    burst = [0.0] * size
    for i in range(600):                                   # 12.5 ms attack/decay at 48 kHz
        env = (i / 60.0) if i < 60 else math.exp(-(i - 60) / 180.0)
        burst[i] = env * math.sin(2.0 * math.pi * 220.0 * i / rate)
    wet = [0.0] * size
    for i in range(size):                                  # convolve burst with the IR
        if burst[i] == 0.0:
            continue
        for pos, amp in [(0, 1.0), (137, 0.7), (409, 0.45), (908, 0.3)]:
            if i + pos < size:
                wet[i + pos] += burst[i] * amp
    rec = deconvolve(wet, burst)
    got = [p for p, _ in early_peaks(rec, rate, span_s=0.03, rel=0.15)]
    want = [0, 137, 409, 908]
    ok_dec = all(any(abs(g - t) <= 2 for g in got) for t in want)
    print(f"  deconvolve  : {'PASS' if ok_dec else 'FAIL'}  (wanted {want}, found {got[:8]})")
    return 0 if (ok_peaks and ok_rt and ok_cons and ok_sp and ok_dec) else 1


# ---------- main ----------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav", nargs="?")
    ap.add_argument("--repeats", type=int, default=8, help="impulses per setting, as passed to measure.py")
    ap.add_argument("--dry-channel", type=int, default=1, metavar="N",
                    help="1-based channel in the FILE carrying the dry impulse (0 = none)")
    ap.add_argument("--wet-channel", type=int, default=3, metavar="N",
                    help="1-based channel in the FILE carrying the module output")
    ap.add_argument("--min-consistency", type=float, default=0.98,
                    help="below this, repeats are NOT averaged — see the note at the top")
    ap.add_argument("--window", type=int, default=16384,
                    help="samples of each repeat to deconvolve (a power of two; 16384 = 341 ms at 48 kHz)")
    ap.add_argument("--selftest", action="store_true", help="check the analysis against a synthetic IR")
    ap.add_argument("--calibrate", action="store_true",
                    help="a take with ONE impulse sent to ALL outputs: report per-channel level and timing")
    ap.add_argument("--ref-channel", type=int, default=None, metavar="N",
                    help="calibration reference channel (default: the lowest carrying signal)")
    args = ap.parse_args()

    if args.selftest:
        sys.exit(selftest())
    if not args.wav:
        sys.exit("give a .wav, or --selftest")

    chans, rate = read_wav(args.wav)

    if args.calibrate:
        print(f"{args.wav}: {len(chans)} channels, {rate} Hz, {len(chans[0])/rate:.1f}s  — CALIBRATION\n")
        calibrate(chans, rate, args.ref_channel)
        return

    # CHANNELS ARE WHERE THE DESK PUTS THEM, not 1-4. A multichannel interface streams its own input
    # numbering, so the G2's outputs land wherever they are patched — on a Qu-24 with the G2 on inputs
    # 5-8, the dry pair is channel 5 and the wet pair channel 7. Hence explicit flags rather than a
    # guess; getting this wrong analyses the wrong signal and says nothing about it.
    def pick(n, what):
        if n <= 0:
            return None
        if n > len(chans):
            sys.exit(f"error: asked for {what} on channel {n} but the file has {len(chans)}")
        return chans[n - 1]

    dry = pick(args.dry_channel, "the dry reference")
    wet = pick(args.wet_channel, "the module output")
    if wet is None:
        sys.exit("error: --wet-channel is required")
    print(f"{args.wav}: {len(chans)} channels, {rate} Hz, {len(wet)/rate:.1f}s")
    print(f"  dry: {'channel %d' % args.dry_channel if dry else 'none — onsets from the wet signal'}"
          f"   wet: channel {args.wet_channel}")

    onsets = find_onsets(dry or wet, rate)
    print(f"impulses found: {len(onsets)}")
    if not onsets:
        sys.exit("no impulses detected — check levels and routing")

    span = min((onsets[i + 1] - onsets[i]) for i in range(len(onsets) - 1)) if len(onsets) > 1 else len(wet)
    groups = [onsets[i:i + args.repeats] for i in range(0, len(onsets), args.repeats)]

    win = min(args.window, span)
    for gi, grp in enumerate(groups):
        starts = [o for o in grp if o + span <= len(wet)]
        if len(starts) < 2:
            continue
        starts = starts[1:]                   # the first of each group may straddle a parameter change
        wets = [wet[o:o + span] for o in starts]

        # RAW consistency: are the recordings themselves repeatable?
        raw_cons = consistency([w[:min(4096, span)] for w in wets], min(4096, span))

        # DECONVOLVED: divide each repeat by ITS OWN dry copy. This is the point of the dry reference —
        # it removes the excitation's shape, so a burst works as well as a click, AND it normalises an
        # excitation that is not identical every time, which makes repeats comparable when the raw
        # recordings are not.
        irs = None
        if dry is not None:
            irs = [align_to_peak(deconvolve(wet[o:o + win], dry[o:o + win])) for o in starts]
            dec_cons = consistency(irs, min(4096, win))
        else:
            dec_cons = None

        print(f"\nsetting {gi}: {len(wets)} usable repeats")
        print(f"  consistency  raw recordings: {raw_cons:+.4f}"
              + (f"   deconvolved: {dec_cons:+.4f}" if dec_cons is not None else "   (no dry reference)"))
        if dec_cons is not None and dec_cons > raw_cons + 0.2:
            print(f"  -> the dry reference is doing its job: the excitation varies but the RESPONSE does not.")

        series, label = (irs, "deconvolved IR") if irs is not None else (wets, "raw wet")
        cons = dec_cons if dec_cons is not None else raw_cons
        if cons is not None and cons >= args.min_consistency:
            n = len(series[0])
            series = [[sum(s[i] for s in series) / len(series) for i in range(n)]]
            print(f"  averaged {len(irs) if irs else len(wets)} repeats ({label})")
        else:
            print(f"  NOT AVERAGED — repeats still differ below {args.min_consistency}; showing repeat 1")
            series = series[:1]

        for x in series:
            pk = early_peaks(x, rate)
            print(f"  early peaks ({label}, samples @ {rate} Hz): {[p for p, _ in pk[:12]]}")
        # RT60 from the RAW wet tail: it needs the full decay, and the -5 dB start point skips the
        # burst anyway, so it does not need the (windowed) deconvolution.
        rt = rt60(wets[0], rate)
        print(f"  RT60 (raw tail): {('%.3f s' % rt) if rt else 'not measurable'}")


if __name__ == "__main__":
    main()
