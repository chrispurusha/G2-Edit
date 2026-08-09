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

import argparse, array, json, math, os, struct, sys, wave


# ---------- reading ----------

def read_wav(path, wanted=None):
    """Channels as lists of floats, and the rate. `wanted` is 1-based channel numbers, or None for all.

    ASK FOR THE CHANNELS YOU NEED. A two minute eight-channel take at 192 kHz is 189 million samples,
    and a Python float costs about 32 bytes once its object header and the list's pointer are counted —
    so materialising all eight channels of one sweep file wants six gigabytes to answer a question
    about two of them. The channels not asked for are never built.
    """
    with wave.open(path, "rb") as w:
        ch, width, rate, frames = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    full = 1 << (8 * width - 1)
    keep = range(ch) if wanted is None else [c for c in range(ch) if (c + 1) in wanted]

    # A 192 kHz capture is tens of millions of samples, and calling int.from_bytes() on each one costs
    # a minute per file. For the widths `array` has a typecode for — which is why capture.c writes
    # 32-bit — the whole file converts in one C-speed call and a channel is then a stride slice. The
    # general path below still handles 24-bit, so an old capture still reads.
    code = {2: "h", 4: "i"}.get(width)
    if code is not None and array.array(code).itemsize == width:
        flat = array.array(code)
        flat.frombytes(raw)
        if sys.byteorder != "little":
            flat.byteswap()
        del raw
        out = [[] for _ in range(ch)]
        for c in keep:
            out[c] = [v / full for v in flat[c::ch]]
        return out, rate

    out = [[] for _ in range(ch)]
    for c in keep:
        out[c] = [0.0] * frames
    step = width * ch
    for i in range(frames):
        base = i * step
        for c in keep:
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


def peak_refine(x, onset, span):
    """Move an onset forward to the largest sample within span — a landmark that does not drift.

    A threshold crossing moves with amplitude and with noise; one sample of drift at 192 kHz is a
    real error in a delay length. The excitation's own peak is the same sample every time, so every
    measurement below is expressed relative to it."""
    hi, at = 0.0, onset
    for i in range(onset, min(onset + span, len(x))):
        if abs(x[i]) > hi:
            hi, at = abs(x[i]), i
    return at


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


def arrivals(ir, rate, engine_rate=96000.0, span_s=0.25, rel=0.10):
    """Distinct energy arrivals in the opening of a response, as a delay tank's stage lengths.

    early_peaks() returns every local maximum, and a capture taken at twice the engine's rate returns
    them in pairs two samples apart — one real arrival appearing as two peaks, because the interface
    is resolving detail the G2 cannot have produced. Worse, a bandlimited arrival rings for several
    samples either side, so a single tap can present as four or five "reflections" and a stage list
    built from that is fiction.

    So peaks are clustered at the resolution the ENGINE has: anything within one engine sample of the
    running cluster is the same arrival, and the cluster is reported at its largest sample. Times come
    back in engine samples as well as milliseconds, because a delay line's length is an integer there
    and that integer is what the engine needs.
    """
    step = max(1.0, rate / engine_rate)
    out = []
    for at, amp in early_peaks(ir, rate, span_s=span_s, rel=rel):
        if out and (at - out[-1][0]) <= step:
            if amp > out[-1][1]:
                out[-1] = (at, amp)
            continue
        out.append((at, amp))
    return [(at, 1000.0 * at / rate, at * engine_rate / rate, amp) for at, amp in out]


def onset_delay(wet, at, rate, noise_s=0.004, margin_db=12.0, search_s=0.2):
    """When the module's output first rises above its own noise floor, relative to the excitation.

    This is the pre-delay, and it is NOT the first entry in arrivals(): that list is thresholded
    relative to the loudest arrival, so a quiet first reflection ahead of a loud cluster is invisible
    in it. A reverb's pre-delay is exactly that quiet-first case.

    The floor is measured from the few milliseconds BEFORE the impulse in the same channel, so it
    includes whatever the tail of the previous repeat left behind and the answer cannot be an artefact
    of a fixed threshold chosen elsewhere.
    """
    lo = max(0, at - int(noise_s * rate))
    ref = wet[lo:at]
    if len(ref) < 16:
        return None
    floor = math.sqrt(sum(v * v for v in ref) / len(ref))
    if floor <= 0.0:
        return None
    thresh = floor * (10.0 ** (margin_db / 20.0))
    for i in range(at, min(at + int(search_s * rate), len(wet))):
        if abs(wet[i]) >= thresh:
            return i - at
    return None


def decimate(x, factor):
    """Average adjacent samples down to a lower rate.

    The G2 computes at 96 kHz, so a 192 kHz capture holds one extra octave of nothing but converter
    noise. Halving the rate before an autocorrelation halves the transform length — which in a
    stdlib FFT is the difference between four seconds per setting and thirty — and the crude two-tap
    average is an adequate anti-alias filter against a band that is already empty.
    """
    factor = int(factor)
    if factor <= 1:
        return list(x)
    return [sum(x[i:i + factor]) / factor for i in range(0, len(x) - factor + 1, factor)]


def periodicities(ir, rate, engine_rate=96000.0, skip_s=0.02, span_s=0.25,
                  min_lag_s=0.0005, max_lag_s=0.12, count=12):
    """Repeating delays in a reverberant tail, found by autocorrelation.

    WHY NOT JUST READ THE PEAKS. A delay tank's early response is only sparse for the first few
    arrivals; after that every stage is feeding every other one and the response is dense. Peak-picking
    a dense response yields a long list of numbers that look like stage lengths and are not — the list
    changes with the threshold, which is the giveaway.

    A recirculating delay leaves a different signature: its length appears as a LAG at which the tail
    correlates with itself, again and again for as long as it rings. That survives density, because it
    is a property of the whole tail rather than of one arrival. An FDN also shows sums and differences
    of its delays, so a lag here is a candidate length and not a proven one — but the shortest strong
    lags are the tank's own periods, and they come back in engine samples ready to compare against a
    delay-line size.

    The direct arrival is skipped: it correlates with everything and would dominate lag zero's
    neighbourhood.
    """
    factor = max(1, int(round(rate / engine_rate)))
    x = decimate(ir[int(skip_s * rate):int((skip_s + span_s) * rate)], factor)
    r = rate / factor
    if len(x) < 64:
        return []

    n = 1
    while n < (2 * len(x)):
        n <<= 1
    re, im = fft(list(x) + [0.0] * (n - len(x)), [0.0] * n)
    # Power spectrum back to the time domain IS the autocorrelation, and one transform each way beats
    # an O(n^2) lag loop by three orders of magnitude at these lengths.
    pr = [re[k] * re[k] + im[k] * im[k] for k in range(n)]
    ac, _ = ifft(pr, [0.0] * n)
    if ac[0] <= 0:
        return []
    ac = [v / ac[0] for v in ac]

    lo, hi = int(min_lag_s * r), min(int(max_lag_s * r), n // 2)
    out = []
    for i in range(lo + 1, hi - 1):
        if ac[i] > ac[i - 1] and ac[i] >= ac[i + 1] and ac[i] > 0.05:
            out.append((i, ac[i]))
    out.sort(key=lambda p: -p[1])
    return [(1000.0 * i / r, i * engine_rate / r, v) for i, v in out[:count]]


def decay_time(ir, rate, block_s=0.05, above_noise_db=6.0, drop_db=3.0, max_span_db=None):
    """Decay time in seconds, from a straight-line fit to the log envelope. Returns (rt60, info).

    WHY NOT SCHROEDER BACKWARD INTEGRATION. Integrating the whole segment assumes the segment ENDS in
    silence. When it ends on a noise floor instead, the integral is dominated by the noise's own total
    energy, which falls off linearly across the window — so the -5 and -35 dB crossings are set by the
    window length rather than by the reverb. The measured symptom was unmistakable once seen: an 8
    second window reported ~15 s for every room, a 20 second window reported ~39 s for every Time
    setting from 42 upwards. Both are twice the window. That is the estimator talking.

    So instead: an RMS envelope in dB, the noise floor taken as the median of the last tenth of the
    segment, and a least-squares fit over the part that is genuinely decaying — from `drop_db` below
    the peak down to `above_noise_db` above the floor. RT60 is 60 dB at the fitted slope, which is an
    extrapolation and says so.

    `info` carries what is needed to disbelieve the answer: the fitted span in dB, its duration, the
    noise floor, and the fit's r^2. A span under about 15 dB or an r^2 below ~0.9 means the decay was
    never visible above the floor and the number should be read as "longer than this window can see".
    """
    n = int(block_s * rate)
    if n < 8 or len(ir) < (20 * n):
        return None, None
    env = []
    for i in range(0, len(ir) - n, n):
        acc = 0.0
        for j in range(i, i + n):
            acc += ir[j] * ir[j]
        env.append(math.sqrt(acc / n))
    peak = max(env)
    if peak <= 0.0:
        return None, None
    db = [20.0 * math.log10(max(v, 1e-12) / peak) for v in env]

    # SMOOTHED BEFORE ANY CROSSING IS LOOKED FOR. A short-block RMS envelope fluctuates by several dB,
    # so "the first block below -3 dB" and "the first below -15 dB" can be neighbours purely by noise,
    # leaving a fit of two points across a span it never really travelled. A five-block moving average
    # costs a quarter of a second of time resolution and makes the crossings mean what they say.
    win = 5
    sm = []
    for i in range(len(db)):
        lo = max(0, i - (win // 2))
        hi = min(len(db), i + (win // 2) + 1)
        sm.append(sum(db[lo:hi]) / (hi - lo))
    db = sm

    tail = sorted(db[int(len(db) * 0.9):])
    floor = tail[len(tail) // 2] if tail else -120.0

    top, bottom = -drop_db, floor + above_noise_db

    # CAPPED SO TWO SOURCES CAN BE COMPARED. A recording stops at its noise floor — the G2's tail clears
    # ours by about 21 dB — while an offline render decays to denormals and offers 190 dB. Fitting each
    # over whatever it happens to offer measures EARLY decay in one case and the whole curve in the
    # other, and if the tail is not a single exponential those are different numbers. --decay-span makes
    # both use the same window.
    if max_span_db is not None and (top - max_span_db) > bottom:
        bottom = top - max_span_db
    if bottom >= top:
        return None, {"span_db": 0.0, "floor_db": floor, "r2": 0.0, "seconds": 0.0}

    start = next((i for i, v in enumerate(db) if v <= top), None)
    if start is None:
        return None, None
    end = next((i for i in range(start, len(db)) if db[i] <= bottom), len(db) - 1)
    if ((end - start) * block_s) < 0.3:
        return None, {"span_db": top - bottom, "floor_db": floor, "r2": 0.0, "seconds": 0.0}

    xs = [(i * n) / rate for i in range(start, end + 1)]
    ys = [db[i] for i in range(start, end + 1)]
    mx = sum(xs) / len(xs)
    my = sum(ys) / len(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    if sxx <= 0.0 or sxy >= 0.0:
        return None, {"span_db": top - bottom, "floor_db": floor, "r2": 0.0, "seconds": xs[-1] - xs[0]}
    slope = sxy / sxx                            # dB per second, negative
    syy = sum((y - my) ** 2 for y in ys)
    r2 = (sxy * sxy) / (sxx * syy) if syy > 0 else 0.0
    return -60.0 / slope, {"span_db": ys[0] - ys[-1], "floor_db": floor, "r2": r2,
                           "seconds": xs[-1] - xs[0]}


def generating_sets(lags, max_size=5, tol=10, multiples=3):
    """Smallest sets of delay lengths whose sums, differences and multiples explain every observed lag.

    WHY A SEARCH AND NOT A FILTER. The obvious approach — drop any lag that equals the sum of two others
    and call what remains the lengths — cannot work, because a set closed under sums and differences
    explains itself both ways round. Measured Small has 6937 = 2338 + 4600 AND 2338 = 6937 - 4600; both
    are true, so a filter that labels one as derived and keeps the other is deciding by iteration order.
    An earlier version of this did exactly that and produced two different, equally confident answers for
    the same data.

    So the question is turned round: which SMALL set of lengths, drawn from the lags themselves, accounts
    for all of them? A recirculating tank puts a lag at each of its lengths, at their multiples, and at
    sums and differences of pairs, so a correct generating set explains everything observed while a wrong
    one leaves lags unaccounted for. Smaller sets are preferred because any large set explains anything.

    VALIDATED ON KNOWN TRUTH: given the lags from an offline render of our own reverb, this recovers
    {2232, 2376, 2554, 2712} — its four actual comb lengths — as the only 4-element set that explains
    them all. That is what makes it worth pointing at hardware data.

    ITS POWER COMES FROM THE RATIO, so read the size before the members. A lag that is IN the candidate
    set explains itself trivially, so a set of 8 drawn from 12 lags predicts only 4 of them and proves
    almost nothing; a set of 4 explaining 12 predicts 8 and is strong. Against the render, 4 explained 12.
    Against the hardware at Time 127 the smallest set that explains Small is EIGHT of its twelve lags,
    which is the method saying it cannot resolve this tank rather than telling you its lengths — the G2
    has more lines than the lags above our noise floor can pin. More level, or more averaging, before
    more searching.

    Returns a list of (set, unexplained_count), best first. An empty result means no set that small
    explains the data, which is itself information: the tank has more lines than max_size.
    """
    lags = sorted(set(int(v) for v in lags))
    if not lags:
        return []

    def explains(cand, target):
        for a in cand:
            for m in range(1, multiples + 1):
                if abs((a * m) - target) <= tol:
                    return True
        for a in cand:
            for b in cand:
                if abs((a + b) - target) <= tol or abs((a - b) - target) <= tol:
                    return True
        return False

    import itertools
    best = []
    for size in range(1, max_size + 1):
        found = []
        for cand in itertools.combinations(lags, size):
            missed = sum(0 if explains(cand, v) else 1 for v in lags)
            if missed == 0:
                found.append((list(cand), 0))
        if found:
            # Prefer the set whose members are the SHORTEST: a tank's own lines generate the long lags as
            # sums, so a set of long lags that happens to explain the short ones by difference is the
            # same arithmetic read backwards and is the less physical reading of the two.
            found.sort(key=lambda p: sum(p[0]))
            return found[:4]
    return best


def rt60(ir, rate):
    """Schroeder backward integration, fitted between -5 and -35 dB and extrapolated to -60.

    SUPERSEDED BY decay_time() for real recordings — see its docstring. Kept because it is the textbook
    method and the self-test's synthetic IR decays into true silence, which is the case it is right for.
    """
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


def deconvolve(wet, dry, eps_db=-60.0, band_db=-60.0):
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

    OUTSIDE THE EXCITATION'S BAND THE INVERSE IS NOT ILL-CONDITIONED, IT IS UNDEFINED, and a floored
    denominator still returns something for it — noise divided by eps, which is large. Capturing a
    96 kHz-clocked G2 at 192 kHz makes that half the spectrum: above ~48 kHz the dry reference holds
    nothing but converter noise, and passing it through the same 1/eps gain buried the real response
    under it. Deconvolved repeats then correlated at -0.06 while the raw ones managed +0.75. So bins
    whose dry magnitude falls below `band_db` of the peak are ZEROED, not floored: the result is
    band-limited to where the measurement has support, which is the honest extent of it.

    ZEROING IS NOT FREE, so the default only rejects what the floor would have amplified anyway.
    Band-limiting turns each delta in the answer into a sinc, and a sinc's side lobes read as extra
    early reflections: at -35 dB the self-test's four known peaks came back as seven, with two of
    them moved by a sample. Tighten it with --band-db only when a capture's excitation genuinely
    occupies a fraction of the recorded bandwidth, and read the extra peaks as the cost of it.
    """
    n = 1
    while n < max(len(wet), len(dry)):
        n <<= 1
    wr = list(wet) + [0.0] * (n - len(wet))
    dr = list(dry) + [0.0] * (n - len(dry))
    wr, wi = fft(wr, [0.0] * n)
    dr, di = fft(dr, [0.0] * n)
    mags = [dr[k] * dr[k] + di[k] * di[k] for k in range(n)]
    peak = max(mags)
    floor = peak * (10.0 ** (eps_db / 10.0))
    band = peak * (10.0 ** (band_db / 10.0))
    qr, qi = [0.0] * n, [0.0] * n
    for k in range(n):
        if mags[k] < band:
            continue                                       # no support here — leave it at zero
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
    ap.add_argument("--band-db", type=float, default=-60.0,
                    help="deconvolution: zero bins whose dry magnitude is below this, relative to its peak. "
                         "Tighten (e.g. -35) only when the excitation occupies a fraction of the recorded "
                         "bandwidth — it band-limits the answer and costs peak resolution")
    ap.add_argument("--raw", action="store_true",
                    help="skip deconvolution and average the RAW recordings. Correct when the excitation is "
                         "a deterministic click: the excitation-repeatability figure is the licence for it")
    ap.add_argument("--engine-rate", type=float, default=96000.0,
                    help="the rate the DEVICE computes at (96 kHz for the G2). Arrival times are reported in "
                         "its samples as well as ms, and peaks closer together than one of them are treated "
                         "as one arrival — a capture at 192 kHz cannot resolve detail the G2 never made")
    ap.add_argument("--decay-span", type=float, default=None, metavar="DB",
                    help="cap the decay fit to this many dB below the peak. Needed to compare a recording "
                         "against an offline render: the recording stops at its noise floor and the render "
                         "does not, so without a cap one measures early decay and the other measures all of it")
    ap.add_argument("--skip", type=int, default=1, metavar="N",
                    help="repeats to discard at the start of each setting. Raise it when the previous "
                         "setting's decay is longer than one impulse period, or its tail is averaged in")
    ap.add_argument("--tail-span", type=float, default=0.25, metavar="S",
                    help="seconds of tail fed to the autocorrelation. Longer resolves closer delays and costs "
                         "transform time; the default is a compromise that runs in seconds per setting")
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

    # Calibration looks at every channel to say which is which; a measurement needs only the two or
    # three it was told about, and at sweep sizes that distinction is gigabytes.
    wanted = None if args.calibrate else {n for n in (args.dry_channel, args.wet_channel, args.ref_channel) if n and n > 0}
    chans, rate = read_wav(args.wav, wanted)

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

    if dry is not None:
        onsets = [peak_refine(dry, o, int(rate * 0.002)) for o in onsets]

    span = min((onsets[i + 1] - onsets[i]) for i in range(len(onsets) - 1)) if len(onsets) > 1 else len(wet)

    # GROUPING BY TIME, NOT BY COUNT, whenever the sidecar says when each setting was applied.
    #
    # Counting assumes every impulse was detected. Miss one — a weak repeat, a threshold that was 2%
    # too high — and every group after it is shifted, so one impulse from the NEXT setting lands inside
    # the previous setting's average. That is not a small error: it averages two different rooms and
    # reports the result as one. It happened here (18 of 20 detected) and the only visible symptom was
    # a final group with one repeat in it, which reads as an off-by-one rather than as contamination.
    #
    # The driver already records the plan, so the file's own time base settles it: setting i owns the
    # window [settle + i*dwell, settle + (i+1)*dwell). A missed impulse then costs one repeat from its
    # own group and nothing from anyone else's.
    labels = None
    side = os.path.splitext(args.wav)[0] + ".json"
    if os.path.exists(side):
        try:
            with open(side) as f:
                plan = json.load(f)
            dwell  = float(plan["period"]) * int(plan["repeats"])
            settle = float(plan.get("settle", 2.0))
            values = [s["value"] for s in plan["settings"]]
            groups, labels = [], []
            for i, v in enumerate(values):
                lo = (settle + (i * dwell)) * rate
                hi = (settle + ((i + 1) * dwell)) * rate
                groups.append([o for o in onsets if lo <= o < hi])
                labels.append(v)
            print(f"  grouped by the sidecar's plan: {len(values)} settings x {dwell:.0f}s"
                  f"  ({[len(g) for g in groups]} impulses found per setting, {plan['repeats']} fired)")
        except (KeyError, ValueError, TypeError) as e:
            print(f"  note: sidecar unusable ({e}) — falling back to grouping by count")
            labels = None
    if labels is None:
        groups = [onsets[i:i + args.repeats] for i in range(0, len(onsets), args.repeats)]

    win = min(args.window, span)
    for gi, grp in enumerate(groups):
        starts = [o for o in grp if o + span <= len(wet)]
        # ENOUGH TO SURVIVE THE SKIP, not an arbitrary two. This was `< 2` from when the first repeat of
        # every group was always discarded; with settings grouped by time and --skip 0 a single impulse
        # per setting is a complete measurement, which is exactly what an offline engine render produces.
        if len(starts) <= args.skip:
            continue
        # THE OPENING REPEATS OF EACH GROUP ARE DISCARDED, and one is not always enough. The first may
        # straddle the parameter change itself, but the real contaminant is the PREVIOUS setting's tail
        # still decaying underneath: with a 15 second RT60 and a 40 second dwell, the old room is
        # audible under the first two impulses of the new one. --skip is how many to drop.
        starts = starts[args.skip:]
        wets = [wet[o:o + span] for o in starts]

        # RAW consistency: are the recordings themselves repeatable?
        raw_cons = consistency([w[:min(4096, span)] for w in wets], min(4096, span))

        # THE EXCITATION'S OWN repeatability, measured on the dry channel. A different question from
        # the two figures below, and the one to read first: it says whether what we sent into the
        # module was the same thing each time. A click should sit near 1.0. A noise burst, or an
        # oscillator whose phase is not reset per note, will not — and that is not a fault, it is
        # exactly why each repeat is deconvolved against ITS OWN dry copy rather than a stored one.
        # If this is high and the wet figure is low, the difference is the module's noise floor, not
        # the trigger.
        dry_cons = (consistency([dry[o:o + min(4096, span)] for o in starts], min(4096, span))
                    if dry is not None else None)

        # DECONVOLVED: divide each repeat by ITS OWN dry copy. This is the point of the dry reference —
        # it removes the excitation's shape, so a burst works as well as a click, AND it normalises an
        # excitation that is not identical every time, which makes repeats comparable when the raw
        # recordings are not.
        irs = None
        if dry is not None and not args.raw:
            irs = [align_to_peak(deconvolve(wet[o:o + win], dry[o:o + win], band_db=args.band_db)) for o in starts]
            dec_cons = consistency(irs, min(4096, win))
        else:
            dec_cons = None

        name = f"value {labels[gi]}" if labels else f"group {gi}"
        print(f"\nsetting {gi} ({name}): {len(wets)} usable repeats")
        if dry_cons is not None:
            print(f"  excitation repeatability (dry channel): {dry_cons:+.4f}"
                  + ("  — the trigger is deterministic; raw averaging is valid"
                     if dry_cons >= 0.98 else "  — the excitation VARIES between repeats"))
        print(f"  consistency  raw recordings: {raw_cons:+.4f}"
              + (f"   deconvolved: {dec_cons:+.4f}" if dec_cons is not None else "   (no dry reference)"))
        if dec_cons is not None and dec_cons > raw_cons + 0.2:
            print(f"  -> the dry reference is doing its job: the excitation varies but the RESPONSE does not.")

        series, label = (irs, "deconvolved IR") if irs is not None else (wets, "raw wet")
        # WHICH FIGURE LICENSES AVERAGING. The gate exists because averaging repeats that differ in
        # PHASE cancels signal rather than noise, and the thing whose phase must repeat is the
        # EXCITATION — which the dry channel measures directly. The wet figure asks something else:
        # whether the module returned the same thing twice. It legitimately does not have to.
        #
        # A reverb answered +0.75 here with a +0.99 excitation, spaced further apart than its own
        # decay. Two things do that and neither is a reason to refuse: a tail longer than the spacing
        # leaves residue under the next impulse, and a modulated delay line never repeats exactly.
        # Both are UNCORRELATED with the impulse, so averaging removes them — which is the whole
        # point. So with a dry reference the excitation figure decides, and the wet figure is
        # reported as what it is: a measurement of how time-varying the module is.
        cons = dry_cons if (dry_cons is not None) else (dec_cons if dec_cons is not None else raw_cons)
        which = "excitation" if (dry_cons is not None) else ("deconvolved" if dec_cons is not None else "raw")
        if cons is not None and cons >= args.min_consistency:
            n = len(series[0])
            series = [[sum(s[i] for s in series) / len(series) for i in range(n)]]
            print(f"  averaged {len(irs) if irs else len(wets)} repeats ({label}), on {which} consistency {cons:+.4f}"
                  f"  [noise floor down ~{10.0 * math.log10(len(wets)):.0f} dB]")
        else:
            print(f"  NOT AVERAGED — {which} consistency {cons:+.4f} is below {args.min_consistency}; showing repeat 1")
            series = series[:1]

        # Pre-delay, measured on the RAW recording at its own onset: averaging shifts nothing but the
        # threshold that finds it, and a per-repeat median is proof the figure is not one stray repeat.
        ons = [onset_delay(wet, o, rate) for o in starts]
        ons = sorted(v for v in ons if v is not None)
        if ons:
            mid = ons[len(ons) // 2]
            print(f"  pre-delay (first output above its own noise floor):"
                  f" {1000.0 * mid / rate:.3f} ms = {mid * args.engine_rate / rate:.1f} @ "
                  f"{args.engine_rate/1000:.0f}k   (spread {1000.0*(ons[-1]-ons[0])/rate:.3f} ms over {len(ons)} repeats)")

        for x in series:
            arr = arrivals(x, rate, engine_rate=args.engine_rate)
            print(f"  arrivals ({label}) — delay after the excitation peak:")
            if not arr:
                print(f"    none above {int(100 * 0.10)}% of peak — check level and routing")
            for at, ms, es, amp in arr[:8]:
                print(f"    {ms:8.3f} ms   {es:9.1f} @ {args.engine_rate/1000:.0f}k   {20 * math.log10(max(amp, 1e-9)):6.1f} dB")

            per = periodicities(x, rate, engine_rate=args.engine_rate, span_s=args.tail_span)
            print(f"  recirculating delays (tail autocorrelation, strongest first):")
            for ms, es, v in per:
                print(f"    {ms:8.3f} ms   {es:9.1f} @ {args.engine_rate/1000:.0f}k   r={v:.3f}")
            if not per:
                print("    none — the tail has no repeating structure above the noise")
        # RT60 from the RAW wet tail: it needs the full decay, and the -5 dB start point skips the
        # burst anyway, so it does not need the (windowed) deconvolution.
        rt, info = decay_time(wets[0], rate, max_span_db=args.decay_span)
        if rt is None or info is None:
            print(f"  decay: not measurable in this window")
        else:
            # RT60 IS ALWAYS AN EXTRAPOLATION HERE and the honest thing is to say by how much rather
            # than to pass or fail it. A 14 dB fit with r2 of 0.997 is a straight line by any standard;
            # it is being stretched to 60 dB, which is worth stating and is not the same as being wrong.
            # What would make it wrong is a short span with a poor fit, and that is what the warning is for.
            stretch = 60.0 / max(info["span_db"], 0.1)
            trust = "" if (info["span_db"] >= 10.0 and info["r2"] >= 0.99) else \
                    "   <- SUSPECT: too little decay above the noise floor to fit"
            print(f"  decay: RT60 {rt:.2f} s  (fitted over {info['span_db']:.1f} dB in "
                  f"{info['seconds']:.2f} s, r2 {info['r2']:.3f}, floor {info['floor_db']:.1f} dB, "
                  f"extrapolated x{stretch:.1f}){trust}")


if __name__ == "__main__":
    main()
