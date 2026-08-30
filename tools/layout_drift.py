#!/usr/bin/env python3
"""Rank module faces by how far their controls sit from the original editor's arrangement.

    ./layout_drift.py                 # the ranked table
    ./layout_drift.py FltClassic      # one module, control by control

WHAT IT COMPARES. Every positional row in moduleResources.h is resolved to a point in the module's
own pixel space using adjust_rectangle()'s anchor rules, and matched to the original's control with
the same CodeRef - which is our parameter index for knobs and buttons, our mode index for a
PartSelector, and our connector I/O index for an Input or Output (see module-layout-rules.md).

WHY IT RANKS BY SPREAD AND NOT BY DISTANCE. The original's XPos/YPos may not be measured from the
same corner of a control as ours, and some faces have been deliberately repositioned. Either shows
up as the SAME offset on every control of a module, which says nothing about arrangement. So the
median offset is subtracted first and the ranking is the residual: how far the controls have moved
RELATIVE TO EACH OTHER. A module with a large median and a tiny residual is laid out like the
original, just shifted; a large residual is a face that is genuinely arranged differently.

Requires tools/rsrc_layout.py beside it.
"""
import argparse, importlib.util, os, re, statistics, sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("rl", os.path.join(HERE, "rsrc_layout.py"))
rl = importlib.util.module_from_spec(spec); spec.loader.exec_module(rl)

MODULE_WIDTH, MODULE_Y_SPAN, MODULE_Y_GAP = 350.0, 43.0, 5.0
PARAMISH = ("Knob", "ButtonFlat", "ButtonText", "ButtonIncDec", "ButtonRadioEdit", "Slider")

def resolve(x, y, w, h, anchor, modH):
    """Percentages + anchor -> the control's top-left in module pixels. Mirrors adjust_rectangle()."""
    s = MODULE_WIDTH / 100.0
    px, py, pw, ph = x * s, y * s, w * s, h * s
    if   anchor.endswith("Right"):  ax = MODULE_WIDTH + px - pw
    elif anchor.endswith("Middle"): ax = (MODULE_WIDTH / 2.0) + px - (pw / 2.0)
    else:                           ax = px
    if   anchor.startswith("anchorTop"):    ay = py
    elif anchor.startswith("anchorMiddle"): ay = (modH / 2.0) + py - (ph / 2.0)
    else:                                   ay = modH + py - ph
    return ax, ay

def ours(path="src/moduleResources.h"):
    """module name -> {'param':[(x,y)], 'mode':[...], 'in':[...], 'out':[...]}"""
    src = open(path, errors="replace").read()
    heights = {m.group(1): int(m.group(2))
               for m in re.finditer(r'\{"([^"]+)",\s*(\d+),', re.search(
                   r'gModuleProperties\[\]\s*=\s*\{(.*?)\n\};', src, re.S).group(1))}
    enum_of = {}
    for m in re.finditer(r'\{"([^"]+)",\s*\d+,', re.search(
            r'gModuleProperties\[\]\s*=\s*\{(.*?)\n\};', src, re.S).group(1)):
        enum_of[re.sub(r'[^a-z0-9]', '', m.group(1).lower())] = m.group(1)

    ROW = re.compile(r'\{(moduleType\w+),\s*(connectorDir\w+)?[^{]*\{\{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}\s*,'
                     r'\s*\{([^}]*)\}\s*\}\s*,\s*(anchor\w+)')
    tables = {}
    for tbl in ("paramLocationList", "connectorLocationList", "modeLocationList"):
        block = re.search(tbl + r'\[\]\s*=\s*\{(.*?)\n\};', src, re.S)
        tables[tbl] = block.group(1) if block else ""

    out = {}
    def enum_to_name(e):
        # "to" ONLY between digits - moduleTypeSw1to2 is Sw1-2, but a blanket replace turns
        # moduleTypeOperator into "opera-r" and silently drops the module from the sweep.
        k = re.sub(r'(?<=\d)to(?=\d)', '-', e.replace("moduleType", ""))
        k = re.sub(r'[^a-z0-9]', '', k.lower())
        return enum_of.get(k)

    for tbl, key in (("paramLocationList", "param"), ("modeLocationList", "mode")):
        for m in ROW.finditer(tables[tbl]):
            nm = enum_to_name(m.group(1))
            if nm is None or nm not in heights: continue
            size = m.group(5)
            w, h = (7.0, 7.0) if "CONNECTOR_SIZE" in size else tuple(
                float(v) for v in (size.split(",") + ["7"])[:2])
            H = heights[nm] * MODULE_Y_SPAN - MODULE_Y_GAP
            out.setdefault(nm, {}).setdefault(key, []).append(
                resolve(float(m.group(3)), float(m.group(4)), w, h, m.group(6), H))
    for m in ROW.finditer(tables["connectorLocationList"]):
        nm = enum_to_name(m.group(1))
        if nm is None or nm not in heights or not m.group(2): continue
        H = heights[nm] * MODULE_Y_SPAN - MODULE_Y_GAP
        key = "in" if m.group(2).endswith("In") else "out"
        out.setdefault(nm, {}).setdefault(key, []).append(
            resolve(float(m.group(3)), float(m.group(4)), 7.0, 7.0, m.group(6), H))
    return out, heights

def theirs(rec):
    """One .rsrc module -> {'param':{code:(x,y)}, 'mode':..., 'in':..., 'out':...} in OUR pixels."""
    rows = rec["height"] or 1
    sx = MODULE_WIDTH / 255.0
    sy = (rows * MODULE_Y_SPAN - MODULE_Y_GAP) / (rows * 15.0)
    got = {"param": {}, "mode": {}, "in": {}, "out": {}}
    for tag, f in rec["controls"]:
        if "XPos" not in f or "CodeRef" not in f: continue
        code = int(f["CodeRef"]); pt = (int(f["XPos"]) * sx, int(f["YPos"]) * sy)
        if   tag in PARAMISH:      got["param"].setdefault(code, pt)
        elif tag == "PartSelector":got["mode"].setdefault(code, pt)
        elif tag == "Input":       got["in"].setdefault(code, pt)
        elif tag == "Output":      got["out"].setdefault(code, pt)
    return got

def compare(name, mine, rec):
    t = theirs(rec); pairs = []
    for key in ("param", "mode", "in", "out"):
        for idx, pt in enumerate(mine.get(key, [])):
            if idx in t[key]:
                pairs.append((key, idx, pt, t[key][idx]))
    if len(pairs) < 2: return None
    dxs = [b[0] - a[0] for _, _, a, b in pairs]
    dys = [b[1] - a[1] for _, _, a, b in pairs]
    mx, my = statistics.median(dxs), statistics.median(dys)
    res = [((dx - mx) ** 2 + (dy - my) ** 2) ** 0.5 for dx, dy in zip(dxs, dys)]
    # CONNECTORS ARE THE CLEANER SIGNAL. A knob or a connector is small, so where its XPos/YPos is
    # measured from barely matters; a tall control - a sequencer's sliders above all - can be
    # measured from its top in one editor and its base in the other, and that shows up as a large,
    # UNIFORM residual that is a convention difference and not a layout difference. SeqVal's 47
    # sliders all landing within a pixel of the same 133 px residual is exactly that. So report the
    # connector-only spread beside the overall one and prefer it when the two disagree.
    cres = [r for (k, _, _, _), r in zip(pairs, res) if k in ("in", "out")]
    return {"n": len(pairs), "median": (mx, my), "spread": statistics.median(res),
            "worst": max(res), "pairs": pairs, "dx": dxs, "dy": dys, "res": res,
            "cn": len(cres), "cspread": statistics.median(cres) if cres else None}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("module", nargs="?")
    ap.add_argument("--min", type=int, default=3, help="skip faces with fewer matched controls")
    args = ap.parse_args()

    mods = rl.read_modules(rl.DEFAULT_RSRC)
    mine, heights = ours()
    rows, ambiguous, unmatched = [], [], []
    for name, m in sorted(mine.items()):
        if name not in mods: unmatched.append(name); continue
        cands = mods[name]
        if len(cands) > 1:
            want = len(m.get("in", [])) + len(m.get("out", []))
            cands = [r for r in cands
                     if sum(1 for t, f in r["controls"] if t in ("Input", "Output")) == want] or cands
            if len(cands) > 1: ambiguous.append(name); continue
        if cands[0]["height"] != heights[name]: pass
        c = compare(name, m, cands[0])
        if c and c["n"] >= args.min: rows.append((c["spread"], c["worst"], c["n"], name, c))

    if args.module:
        for sp, wo, n, name, c in rows:
            if name != args.module: continue
            print("%s - %d controls matched, median offset (%+.1f, %+.1f) px" %
                  (name, n, c["median"][0], c["median"][1]))
            print("  %-8s %-4s %18s %18s %10s" % ("what", "idx", "ours (x,y)", "theirs (x,y)", "residual"))
            for (key, idx, a, b), r in zip(c["pairs"], c["res"]):
                print("  %-8s %-4d %8.1f %8.1f %9.1f %8.1f %10.1f" % (key, idx, a[0], a[1], b[0], b[1], r))
            return 0
        print("no data for %r" % args.module); return 1

    rows.sort(reverse=True)
    print("%-14s %7s %8s %8s %6s %8s   %s" %
          ("module", "matched", "spread", "worst", "conns", "conn-spr", "median offset"))
    for sp, wo, n, name, c in rows:
        print("%-14s %7d %8.1f %8.1f %6d %8s   (%+.0f, %+.0f)" %
              (name, n, sp, wo, c["cn"],
               ("%.1f" % c["cspread"]) if c["cspread"] is not None else "-",
               c["median"][0], c["median"][1]))
    print("\n%d faces compared. spread/worst are PIXELS after removing each module's median offset." % len(rows))
    if ambiguous: print("ambiguous names, skipped: %s" % ", ".join(ambiguous))
    if unmatched:
        print("%d of our modules have no face in the .rsrc: %s%s" %
              (len(unmatched), ", ".join(sorted(unmatched)[:12]), " ..." if len(unmatched) > 12 else ""))
    thin = [n for n in mine if n in mods and n not in [r[3] for r in rows] and n not in ambiguous and n not in unmatched]
    if thin: print("%d matched too few controls to rank: %s%s" %
                   (len(thin), ", ".join(sorted(thin)[:12]), " ..." if len(thin) > 12 else ""))
    return 0

if __name__ == "__main__":
    sys.exit(main())
