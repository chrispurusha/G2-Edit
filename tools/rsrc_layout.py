#!/usr/bin/env python3
"""Read module faces out of the original editor's resource file.

    ./rsrc_layout.py --list
    ./rsrc_layout.py Delay
    ./rsrc_layout.py Delay --compare

THE FILE'S STRUCTURE IS THE WHOLE TRICK, and getting it wrong is silent rather than loud - it
yields plausible numbers that are simply another module's. Three sessions' worth of wrong answers
came out of it before the shape was understood:

  - A "<#Module" block is opened but NEVER CLOSED. It ends where the next one begins.
  - NAMES ARE NOT UNIQUE. Two different modules are both called "Delay" - FileName "LogicDelay" at
    height 2 and FileName "FXDelay" at height 3. Keying a dict on Name alone silently keeps the last
    and hands you the wrong module; it briefly cost this project a wrong module height. Modules are
    returned keyed by "Name" AND listed with their FileName, and duplicates are reported.
  - A block can hold SEVERAL modules. Each starts at its own indent-2 `Name:` line - the DelayQuad
    block holds six (DelayQuad, DlyClock, DlyShiftReg, DelayA, DelayB, DlyEight), the Eq3band block
    seven. So SPLIT ON indent-2 `Name:`, never on "<#Module".
  - Controls sit at indent 2 inside the module they follow; their fields are indented further.

Parsed that way the file yields 189 modules. Any parse returning ~119 has counted blocks, and any
parse that gives a module more connectors than the instrument reports parameters is wrong - that is
the check that caught it: a quad delay does not have 34 connectors.

WHAT THE FIELDS MEAN
  CodeRef on a control IS our parameter index (paramLocationList order) for knobs and selectors, and
  our connector I/O index for Input/Output. That is what makes a port mechanical rather than a guess.
  InfoFunc is the id of the original's text formatter for that control - useful when a readout's
  units are in doubt.

THE COORDINATE TRANSFORM is documented in Docs/module-layout-rules.md and implemented below. It is
deliberately ANISOTROPIC: the original's space is 255 wide x 15 per row, ours is 350 wide x 43 per
row less the title bar, so our faces are about 2.1x taller relative to their width. Arrangement is
preserved; proportion is not. Do not paste the output in blind - see the same doc.
"""
import argparse, os, re, sys

DEFAULT_RSRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                            "Original Editor", "EditorResources", "Nord Modular G2 Editor.rsrc")
POSITIONAL = ("Knob", "PartSelector", "ButtonText", "ButtonFlat", "ButtonIncDec",
              "ButtonRadioEdit", "TextField", "Led", "Input", "Output", "Graph",
              "TextEdit", "Bitmap", "Line", "Text")

def read_modules(path):
    """name -> {'height': int, 'controls': [(tag, {field: value})]}"""
    with open(path, "rb") as f:
        raw = f.read()
    # The resource fork is binary with embedded text; pull printable runs the way `strings` does.
    text = re.sub(rb"[^\x09\x0a\x20-\x7e]+", b"\n", raw).decode("ascii", "replace")
    lines = text.split("\n")

    starts = [i for i, l in enumerate(lines) if re.match(r"  Name:\"", l)]
    starts.append(len(lines))
    mods = {}
    for a, b in zip(starts, starts[1:]):
        name = re.match(r"  Name:\"(.*)\"", lines[a]).group(1)
        height, controls, tag, fields = None, [], None, {}
        for l in lines[a:b]:
            m = re.match(r"  Height:(\d+)", l)
            if m and height is None:
                height = int(m.group(1)); continue
            m = re.match(r"  <#(\w+)", l)
            if m:
                if tag: controls.append((tag, fields))
                tag, fields = m.group(1), {}
                continue
            if l.strip() == "#>":
                if tag: controls.append((tag, fields))
                tag, fields = None, {}
                continue
            m = re.match(r"\s+([\w ]+):\"?(-?\w+)\"?", l)
            if m and tag: fields[m.group(1).strip()] = m.group(2)
        if tag: controls.append((tag, fields))
        rec = {"height": height, "controls": controls,
               "file": next((v for k, v in _fields(lines[a:b]) if k == "FileName"), None),
               "tip":  next((v for k, v in _fields(lines[a:b]) if k == "Tooltip"), None)}
        mods.setdefault(name, []).append(rec)
    return mods

def _fields(block):
    for l in block:
        m = re.match(r'  (\w+):"(.*)"', l)
        if m: yield m.group(1), m.group(2)

def transform(x, y, rows):
    """Original 255x(15*rows) -> our percentages of MODULE_WIDTH, y=0 at the module top.

       Since the title bar was removed the face has no reserved strip, so the full
       module height (rows * MODULE_Y_SPAN - MODULE_Y_GAP) maps onto the original's."""
    body_pct = (rows * 43.0 - 5.0) * 100.0 / 350.0
    return x * 100.0 / 255.0, y * body_pct / (rows * 15.0)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module", nargs="?")
    ap.add_argument("--rsrc", default=DEFAULT_RSRC)
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--file", help="disambiguate a duplicated Name by its FileName")
    ap.add_argument("--compare", action="store_true", help="show our current rows beside it")
    args = ap.parse_args()

    mods = read_modules(args.rsrc)
    if args.list or not args.module:
        dupes = [n for n in mods if len(mods[n]) > 1]
        print("%d names, %d modules" % (len(mods), sum(len(v) for v in mods.values())))
        if dupes:
            print("DUPLICATE NAMES - disambiguate by FileName: %s" % ", ".join(sorted(dupes)))
        for n in sorted(mods):
            for r in mods[n]:
                print("  %-14s %-12s h=%s  %d controls" % (n, r["file"] or "", r["height"], len(r["controls"])))
        return 0
    if args.module not in mods:
        print("no module %r (try --list)" % args.module); return 1
    cands = mods[args.module]
    if args.file: cands = [r for r in cands if r["file"] == args.file]
    if len(cands) > 1:
        print("%r is AMBIGUOUS - %d modules share the name. Re-run with --file:" % (args.module, len(cands)))
        for r in cands: print("   --file %-12s  h=%s  %r" % (r["file"], r["height"], r["tip"]))
        return 1
    m = cands[0]; rows = m["height"] or 1
    print("%s (%s, %r) - original height %d rows (%d px of body in our space)" %
          (args.module, m["file"], m["tip"], rows, rows * 43 - 5 - 17.5))
    print("%-16s %5s %5s %5s   ->  %7s %7s   %s" %
          ("control", "Code", "xOrig", "yOrig", "x%", "y%", "InfoFunc"))
    for tag, f in m["controls"]:
        if tag not in POSITIONAL or "XPos" not in f: continue
        x, y = int(f["XPos"]), int(f["YPos"])
        xp, yp = transform(x, y, rows)
        print("%-16s %5s %5d %5d   ->  %7.1f %7.1f   %s" %
              (tag, f.get("CodeRef", "-"), x, y, xp, yp, f.get("InfoFunc", "")))
    return 0

if __name__ == "__main__":
    sys.exit(main())
