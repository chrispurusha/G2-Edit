#!/usr/bin/env python3
r"""Drive the G2 through a parameter sweep while recording its audio output.

WHAT THIS IS FOR
    Characterising a module by measurement rather than by guesswork: step one of its parameters over
    a range on the HARDWARE, record the result, and let analyse_ir.py turn that into numbers. The
    engine's own "KNOWN GAPS" list (soundEngine.h) is mostly questions this can answer — reverb stage
    lengths, filter responses, envelope times, static transfer curves.

THE PATCH IT EXPECTS
    A free-running impulse train through the module under test, with the raw impulse sent out a SECOND
    output pair — the G2 has four outputs, so both fit in one take:

        LfoA (slow, ~1 Hz) --> Pulse (shortest time) --+--> [module] --> out 3-4   (wet)
                                                       \-------------> out 1-2   (dry reference)

    The Pulse gives a one-sample click, a Dirac at the engine's own rate. Set the module fully wet
    where it has a dry path, and make sure NOTHING else in the patch sounds.

    THE DRY PAIR IS WORTH THE CABLE. It locates every impulse to the sample instead of the analyser
    inferring position from the processed signal; it is the system reference (converters, cabling,
    interface) captured in the SAME take, so deconvolving with it cannot drift; and comparing dry
    repeats to one another says whether the excitation repeats at all, independently of the module.

    Wet-only two-channel captures still work, less exactly.

WHY AN IMPULSE TRAIN AND NOT ONE NOTE PER SETTING
    Two reasons. Repeats let the analyser average away the noise floor, and counting impulses rather
    than seconds means the recording needs no clock alignment: ffmpeg's start latency, USB timing and
    scheduler jitter all stop mattering. The driver dwells for a whole number of impulse periods and
    the analyser finds the onsets itself.

    Note-triggered excitation is deliberately NOT the default: a note's start phase is not guaranteed
    to be identical every time, and repeats that are not phase-identical must not be averaged. Use
    --gate only for what needs a gate (envelopes), and read what analyse_ir.py says about consistency
    before trusting any averaged result from it.
"""

import argparse, json, os, subprocess, sys, time

CMD    = "/tmp/g2edit_cmd.txt"
RESULT = "/tmp/g2edit_result.txt"


def backdoor(command, timeout=16.0):
    """Send one backdoor command and return its result text."""
    with open(CMD, "w") as f:
        f.write(command + "\n")
    deadline = time.time() + timeout
    while os.path.exists(CMD) and time.time() < deadline:
        time.sleep(0.05)
    if os.path.exists(CMD):
        raise RuntimeError(f"backdoor did not consume '{command}' — is G2-Edit running with G2_EDIT_BACKDOOR=1?")
    time.sleep(0.05)
    try:
        with open(RESULT) as f:
            return f.read().strip()
    except OSError:
        return ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True, help="output .wav (a .json sidecar is written beside it)")
    ap.add_argument("--input-device", required=True, help="ffmpeg avfoundation audio index, e.g. 0 for QU-24")
    ap.add_argument("--rate", type=int, default=96000)
    ap.add_argument("--channels", type=int, default=4,
                    help="4 = dry on 1-2 and wet on 3-4 (preferred); 2 = wet only")
    ap.add_argument("--loc", default="FX", choices=["VA", "FX", "va", "fx"])
    ap.add_argument("--index", type=int, required=True, help="module index, from the DUMP command")
    ap.add_argument("--param", type=int, required=True, help="parameter index to sweep")
    ap.add_argument("--values", required=True, help="comma-separated, e.g. 0,32,64,96,127")
    ap.add_argument("--period", type=float, default=1.0, help="seconds between impulses (match the LFO)")
    ap.add_argument("--repeats", type=int, default=8, help="impulses per setting; the first is discarded")
    ap.add_argument("--gate", type=int, default=None, metavar="NOTE",
                    help="excite with a note instead of relying on the impulse train (see the caveat above)")
    args = ap.parse_args()

    values = [int(v) for v in args.values.split(",")]
    dwell  = args.period * args.repeats

    # Fail before recording rather than half way through a ten-minute sweep.
    if "OK" not in backdoor("DUMP").split("\n")[0]:
        sys.exit("error: no answer from the editor's backdoor")

    print(f"recording -> {args.out}  ({len(values)} settings x {dwell:.1f}s = {len(values) * dwell:.0f}s)")
    rec = subprocess.Popen(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
         "-f", "avfoundation", "-channels", str(args.channels),
         "-i", f":{args.input_device}",
         "-ar", str(args.rate), "-c:a", "pcm_s24le", args.out],
        stdin=subprocess.DEVNULL)

    plan = {"rate": args.rate, "period": args.period, "repeats": args.repeats,
            "loc": args.loc.upper(), "index": args.index, "param": args.param, "settings": []}
    try:
        time.sleep(2.0)                                   # let the input settle before the first setting
        for v in values:
            reply = backdoor(f"DEVSET {args.loc.upper()} {args.index} {args.param} {v}")
            if not reply.startswith("OK"):
                raise RuntimeError(f"DEVSET rejected: {reply}")
            print(f"  param {args.param} = {v:3}  ({dwell:.1f}s)")
            plan["settings"].append({"value": v, "requested_at": time.time()})
            if args.gate is not None:
                for _ in range(args.repeats):
                    backdoor(f"DEVNOTE {args.gate} 100 on")
                    time.sleep(args.period * 0.5)
                    backdoor(f"DEVNOTE {args.gate} 0 off")
                    time.sleep(args.period * 0.5)
            else:
                time.sleep(dwell)
    finally:
        rec.terminate()
        rec.wait()

    # WHAT WE ACTUALLY GOT, not what we asked for. ffmpeg's avfoundation input has no channel-count
    # option, and on some setups it captures only the first two channels of a multichannel device — in
    # which case a G2 output patched to input 5 is simply not in the file. Checking here turns that into
    # one clear message instead of an analysis that reads the wrong signal and says nothing useful.
    try:
        import wave
        with wave.open(args.out, "rb") as w:
            got, grate, gframes = w.getnchannels(), w.getframerate(), w.getnframes()
        print(f"captured: {got} channels, {grate} Hz, {gframes / max(grate,1):.1f}s")
        if got < args.channels:
            print(f"\nWARNING: asked for {args.channels} channels and got {got}. Any G2 output patched")
            print(f"         above input {got} is NOT in this file. Check what the device presents")
            print(f"         (ffmpeg -f avfoundation -i \":{args.input_device}\" prints its channel count)")
            print(f"         before capturing a full sweep.")
        if grate != args.rate:
            print(f"\nWARNING: asked for {args.rate} Hz and got {grate} Hz — stage lengths will be in")
            print(f"         {grate} Hz samples, so scale them for the engine's 96 kHz domain.")
    except Exception as e:
        print(f"note: could not inspect the recording ({e})")

    side = os.path.splitext(args.out)[0] + ".json"
    with open(side, "w") as f:
        json.dump(plan, f, indent=2)
    print(f"done: {args.out}\n      {side}")
    print("\nSETTING ORDER IS THE GROUND TRUTH, not the timestamps: analyse_ir.py finds the impulse\n"
          "onsets and groups them in {}s, so the sidecar's order is what matters.".format(args.repeats))


if __name__ == "__main__":
    main()
