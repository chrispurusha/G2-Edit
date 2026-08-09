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

        EnvADSR "Env" out --> Pulse (Sub range, Time 0) --+--> [module] --> out 3-4   (wet)
                                                          \-------------> out 1-2   (dry reference)

    The Pulse gives a click a few samples long, a Dirac as near as the engine has one, and the
    envelope's rising edge triggers it — so ONE note per impulse and the excitation is repeatable to
    +0.99 on the dry channel. Nothing else in the patch may sound: the oscillator feeding the
    envelope's audio input is left disconnected from the outputs deliberately.

    THE PULSE MUST BE UP-RATED (its cables turn orange rather than yellow). At control rate the
    module cannot produce a pulse shorter than a control period, so the shortest Time setting simply
    does not exist and the excitation comes out both long and ragged.

    Set the module fully wet where it has a dry path — for the Reverb that is DryWet at 127.

    THE DRY PAIR IS WORTH THE CABLE. It locates every impulse to the sample instead of the analyser
    inferring position from the processed signal; it is the system reference (converters, cabling,
    interface) captured in the SAME take, so deconvolving with it cannot drift; and comparing dry
    repeats to one another says whether the excitation repeats at all, independently of the module.

    Wet-only two-channel captures still work, less exactly.

WHY REPEATS AND NOT ONE IMPULSE PER SETTING
    Two reasons. Repeats let the analyser average away the noise floor, and counting impulses rather
    than seconds means the recording needs no clock alignment: capture start latency, USB timing and
    scheduler jitter all stop mattering. The driver dwells for a whole number of impulse periods and
    the analyser finds the onsets itself.

    A note-gated click IS phase-repeatable in practice — the G2's own Pulse module fires on the
    envelope's rising edge, and measured excitation repeatability on the dry channel is +0.99. What is
    NOT repeatable is a note played on an oscillator whose phase does not reset, so read what
    analyse_ir.py says about excitation consistency before trusting any averaged result.

WHY IT RECORDS THROUGH ./capture AND NOT ffmpeg
    ffmpeg's avfoundation input reports the interface's real rate and then delivers 48 kHz regardless,
    because an AVCaptureSession resamples: a 192 kHz capture came out as a file LABELLED 192000 with a
    quarter of the samples in it, and nothing in the file said so. capture.c goes through the HAL and
    hands back the device's own format. Build it first: see the comment at the top of capture.c.
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
    ap.add_argument("--device", required=True, help="input device name substring, e.g. Fireface (./capture --list)")
    ap.add_argument("--rate", type=int, default=0,
                    help="request this device rate; 0 leaves it wherever it is. What it actually settles at "
                         "is printed and written into the file")
    ap.add_argument("--loc", default="FX", choices=["VA", "FX", "va", "fx"])
    ap.add_argument("--index", type=int, required=True, help="module index, from the DUMP command")
    ap.add_argument("--param", type=int, default=0, help="parameter index to sweep (ignored with --sweep-mode)")
    ap.add_argument("--values", required=True, help="comma-separated, e.g. 0,32,64,96,127")
    ap.add_argument("--period", type=float, default=1.0, help="seconds between impulses (match the LFO)")
    ap.add_argument("--repeats", type=int, default=8, help="impulses per setting; the first is discarded")
    ap.add_argument("--pre", action="append", default=[], metavar="'<VA|FX> <index> <param> <value>'",
                    help="a DEVSET applied once before the sweep starts, repeatable. This is how a second "
                         "parameter is held constant — sweep Time four times, once per reverb Type, and each "
                         "run is its own file rather than one file needing a decoder ring")
    ap.add_argument("--pre-mode", action="append", default=[], metavar="'<VA|FX> <index> <mode> <value>'",
                    help="a DEVMODE applied once before the sweep starts, repeatable. A DROP-DOWN SELECTOR IS "
                         "A MODE, NOT A PARAMETER: the Reverb's Small/Medium/Large/Hall, a filter's slope, an "
                         "oscillator's waveform. Driving one of those through --pre writes a parameter index "
                         "the module does not have, and before DEVSET checked for that it silently produced a "
                         "whole sweep of the same setting")
    ap.add_argument("--sweep-mode", type=int, default=None, metavar="M",
                    help="sweep MODE M instead of a parameter, with --values as its settings")
    ap.add_argument("--gate", type=int, default=None, metavar="NOTE",
                    help="excite with a note instead of relying on the impulse train (see the caveat above)")
    args = ap.parse_args()

    values = [int(v) for v in args.values.split(",")]
    dwell  = args.period * args.repeats

    # Fail before recording rather than half way through a ten-minute sweep.
    if "OK" not in backdoor("DUMP").split("\n")[0]:
        sys.exit("error: no answer from the editor's backdoor")

    for verb, items in (("DEVSET", args.pre), ("DEVMODE", args.pre_mode)):
        for pre in items:
            reply = backdoor(f"{verb} {pre}")
            if not reply.startswith("OK"):
                sys.exit(f"error: {verb} '{pre}' rejected: {reply}")
            print(f"  pre: {verb} {pre}")

    # capture is given the WHOLE duration up front and stops itself, rather than being killed at the
    # end: a terminated recorder can leave a truncated file whose last setting is missing, and this
    # way the settings all sit safely inside one continuous recording.
    settle = 2.0
    total  = settle + (len(values) * dwell) + 1.0
    tool   = os.path.join(os.path.dirname(os.path.abspath(__file__)), "capture")
    if not os.path.exists(tool):
        sys.exit(f"error: {tool} is not built — see the build line at the top of capture.c")

    print(f"recording -> {args.out}  ({len(values)} settings x {dwell:.1f}s = {total:.0f}s total)")
    cmd = [tool, "--device", args.device, "--out", args.out, "--seconds", f"{total:.2f}"]
    if args.rate:
        cmd += ["--rate", str(args.rate)]
    rec = subprocess.Popen(cmd, stdin=subprocess.DEVNULL)

    plan = {"rate": args.rate, "period": args.period, "repeats": args.repeats, "settle": settle,
            "loc": args.loc.upper(), "index": args.index, "param": args.param,
            "pre": args.pre, "settings": []}
    try:
        time.sleep(settle)                                # let the input settle before the first setting
        for v in values:
            if args.sweep_mode is not None:
                reply = backdoor(f"DEVMODE {args.loc.upper()} {args.index} {args.sweep_mode} {v}")
                what  = f"mode {args.sweep_mode}"
            else:
                reply = backdoor(f"DEVSET {args.loc.upper()} {args.index} {args.param} {v}")
                what  = f"param {args.param}"
            if not reply.startswith("OK"):
                raise RuntimeError(f"rejected: {reply}")
            print(f"  {what} = {v:3}  ({dwell:.1f}s)")
            plan["settings"].append({"value": v, "requested_at": time.time()})
            if args.gate is not None:
                # Gate held only long enough for the envelope to rise and fire the Pulse; the rest of
                # the period is silence, so the tail decays into the noise floor rather than into the
                # next note's attack.
                hold = min(0.2, args.period * 0.3)
                for _ in range(args.repeats):
                    backdoor(f"DEVNOTE {args.gate} 100 on")
                    time.sleep(hold)
                    backdoor(f"DEVNOTE {args.gate} 0 off")
                    time.sleep(args.period - hold)
            else:
                time.sleep(dwell)
    except BaseException:
        # FAIL FAST WHEN THE DRIVING SIDE DIES. The recorder was handed the whole duration up front and
        # stops itself, which is right for the normal path — but on the error path `rec.wait()` alone sits
        # out the remaining minutes recording silence before the exception is ever seen. That happened:
        # the editor exited under a running sweep, and a capture that was already known to be worthless
        # ran for four more minutes before saying so. The recording is abandoned rather than kept; a file
        # whose triggers stopped part way through is worse than no file, because it looks analysable.
        rec.terminate()
        rec.wait()
        raise
    else:
        rec.wait()

    # WHAT WE ACTUALLY GOT, not what we asked for. The recorder prints this too, but a sweep is long
    # enough that its first line has scrolled away by the time anyone reads the end, and a capture at
    # the wrong rate scales every measurement in it.
    try:
        import wave
        with wave.open(args.out, "rb") as w:
            got, grate, gframes = w.getnchannels(), w.getframerate(), w.getnframes()
        print(f"captured: {got} channels, {grate} Hz, {gframes / max(grate,1):.1f}s")
        if gframes / max(grate, 1) < (total - 2.0):
            print(f"\nWARNING: the recording is shorter than the {total:.0f}s sweep — the last settings")
            print(f"         are missing from it.")
        if args.rate and grate != args.rate:
            print(f"\nWARNING: asked for {args.rate} Hz and got {grate} Hz — the interface would not")
            print(f"         change rate (external clock, or another app holding it open).")
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
