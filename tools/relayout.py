#!/usr/bin/env python3
"""Re-lay out module faces by rule. Rewrites ONLY the position, anchor and label of the rows a
family's spec names, so the diff shows exactly what moved and a spec can be run again.

    ./relayout.py --list
    ./relayout.py Level          # then ../do-uncrustify, build, and ./face-shots to look

Rows are addressed by module and by their ORDER among that module's rows in one table. That order IS
the parameter index (paramLocationList) or the connector order (connectorLocationList), so a spec can
move a control but never turn it into a different parameter; a spec giving the wrong number of rows
for a module is refused rather than half-applied. The rules behind the numbers are
Docs/module-layout-rules.md, "The common face"."""
import os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
RES  = os.path.join(HERE, "..", "src", "moduleResources.h")
TYPES = os.path.join(HERE, "..", "src", "types.h")

BL, BR, TL, TR, MR = "anchorBottomLeft", "anchorBottomRight", "anchorTopLeft", "anchorTopRight", "anchorMiddleRight"
MM = "anchorMiddle"
KEEP = object()

def at(x, y, anchor, label=KEEP, loc=KEEP):
    """A row's new place. label is a C literal ('"Mod"' or 'NULL'); KEEP leaves a field alone."""
    return (x, y, anchor, label, loc)

IN_TOP_RIGHT  = at(-3, 3, TR)     # the main input
OUT_BOT_RIGHT = at(-3, -3, BR)    # the main output

# family -> (params, connectors); each maps a module name to its rows in table order
FAMILIES = {
    "Level": ({
        "Constant":  [at(70, -3, BL), at(0, 0, MM)],     # the owner's own, centred selector (93e1df0)
        "ConstSwT":  [at(44, -3, BL), at(28, -1, BL), at(12, -1, BL)],
        "ConstSwM":  [at(44, -3, BL), at(28, -1, BL), at(12, -1, BL)],
        "CompLev":   [at(28, -3, BL)],
        "LevAdd":    [at(28, -3, BL), at(12, -1, BL)],
        "LevAmp":    [at(28, -3, BL), at(12, -1, BL)],
        "LevConv":   [at(44, -1, BL, '"Out"'), at(28, -1, BL, '"In"')],
        "LevMod":    [at(28, -3, BL, '"Depth"'), at(44, -3, BL, '"Type"')],
        "EnvFollow": [at(28, -3, BL), at(44, -3, BL)],
        "ModAmt":    [at(44, -3, BL), at(60, -1, BL), at(3, 6, TL), at(12, -1, BL)],
        "NoiseGate": [at(44, -3, BL), at(12, -3, BL), at(28, -3, BL), at(-3, 0, MR)],
    }, {
        "Constant":  [OUT_BOT_RIGHT],
        "ConstSwT":  [OUT_BOT_RIGHT],
        "ConstSwM":  [OUT_BOT_RIGHT],
        "CompLev":   [at(-3, 3, TR, '"A"', "labelLocLeft"), at(-3, -3, BR, '"A>=C"', "labelLocLeft")],
        "CompSig":   [at(-17, 3, TR, '"A"', "labelLocLeft"), at(-3, 3, TR, '"B"', "labelLocLeft"),
                      at(-3, -3, BR, '"A>=B"', "labelLocLeft")],
        "LevAdd":    [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "LevAmp":    [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "LevConv":   [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "EnvFollow": [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "LevMult":   [IN_TOP_RIGHT, at(3, -3, BL, '"Mod"', "labelLocRight"), OUT_BOT_RIGHT],
        "LevMod":    [IN_TOP_RIGHT, at(3, -3, BL, '"Mod"', "labelLocUp"), at(21, -3, BL, '"-"', "labelLocRight"),
                      OUT_BOT_RIGHT],
        "MinMax":    [at(-17, 3, TR, '"A"', "labelLocLeft"), at(-3, 3, TR, '"B"', "labelLocLeft"),
                      at(-17, -3, BR, '"Min"', "labelLocLeft"), at(-3, -3, BR, '"Max"', "labelLocLeft")],
        "ModAmt":    [IN_TOP_RIGHT, at(37, -3, BL, '"-"', "labelLocRight"), OUT_BOT_RIGHT],
        "NoiseGate": [IN_TOP_RIGHT, OUT_BOT_RIGHT, at(-17, -3, BR, '"Env"', "labelLocLeft")],
        "Red2Blue":  [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "Blue2Red":  [IN_TOP_RIGHT, OUT_BOT_RIGHT],
    }),
    # The mixers are mostly the owner's approved faces, so this touches only what he asked for: Exp in
    # one slot, straight after the Chain; Mix2-1A/B's channels 8% left, clear of the Out and lined up
    # with each other; and Mix8-1A's inputs 5% left, off the meter they overlapped.
    "Mixer": ({
        "Mix1-1A":  [None, None, at(21, 6, TL)],
        "Mix1-1S":  [None, None, at(33, 6, TL)],
        "Mix2-1A":  [at(39, 14, TL), at(13, 16, TL), at(74, 14, TL), at(48, 16, TL), None],
        "Mix2-1B":  [at(22, 16, TL), at(39, 14, TL), at(56, 16, TL), at(74, 14, TL), None],
        "Mix8-1A":  [None],
        "Mix8-1B":  [None] * 8 + [at(3, 9, TL, '"Curve"'), at(16, 9, TL)],
    }, {
        "Mix2-1A":  [at(32, -3, BL), at(67, -3, BL), None, None],
        "Mix2-1B":  [at(32, -3, BL), at(67, -3, BL), None, None],
        "Mix8-1A":  [at(16 + (9 * n), -3, BL) for n in range(8)] + [None],
    }),
    # The owner's DelayB and DlyStereo set the delay template: columns 19/36/53/70, Range top-left,
    # the Time/Clk selector over the Time dial, In top-right, Bypass middle-right, Out bottom-right,
    # mod jacks paired to their dials with "-". DlySingleA/B, DelayB and DlyStereo are his and are
    # not touched. Tapped delays end their taps at the bottom-right corner, 8% apart.
    "Delay": ({
        "DelayDual": [at(36, -3, BL), at(19, -3, BL), at(70, -3, BL), at(53, -3, BL)],
        "DelayQuad": [p for c in (19, 36, 53, 70) for p in (at(c, 24, TL), at(c, -3, BL))] + [at(19, 10, TL)],
        "DlyEight":  [at(19, -3, BL)],
        "DlyClock":  [at(28, -3, BL)],
        "DelayA":    [at(19, -3, BL), at(36, -3, BL), at(53, -3, BL), at(70, -3, BL), at(-3, 0, MR), at(19, -15, BL)],
    }, {
        "DelayDual": [at(-3, 3, TR, "NULL", "labelLocUp"), at(12, -3, BL, '"-"', "labelLocRight"),
                      at(46, -3, BL, '"-"', "labelLocRight"), at(-10, -3, BR), OUT_BOT_RIGHT],
        "DelayQuad": [IN_TOP_RIGHT] + [at(c - 7, -3, BL, '"-"', "labelLocRight") for c in (19, 36, 53, 70)]
                     + [OUT_BOT_RIGHT] + [at(c + 9, 26, TL) for c in (19, 36, 53, 70)],
        "DlyEight":  [IN_TOP_RIGHT] + [at(-(3 + 8 * (8 - k)), -3, BR, '"%d"' % k, "labelLocUp") for k in range(1, 9)],
        "DlyShiftReg": [IN_TOP_RIGHT, None] + [at(-(3 + 8 * (8 - k)), -3, BR) for k in range(1, 9)],
        "DlyClock":  [IN_TOP_RIGHT, None, OUT_BOT_RIGHT],
        "DelayA":    [IN_TOP_RIGHT, OUT_BOT_RIGHT],
    }, {
        # The range selectors are MODES (modeLocationList), not parameters.
        "DelayDual": [at(3, 9, TL, '"Range"')],
        "DelayQuad": [at(3, 10, TL, '"Range"')],
        "DlyEight":  [at(3, 9, TL, '"Range"')],
        "DelayA":    [at(3, 9, TL, '"Range"')],
    }),
    # KeyQuant: Capture and Range in the bottom band, clear of a 16-W name; the twelve notes are
    # the keyboard's keys and their own rows are never drawn (moduleGraphics.c notes §84).
    # Parameter order is the G2's: Range, Capture, then E F F# G G# A A# B C C# D D#.
    "Note": ({
        "KeyQuant": [at(28, -3, BL), at(12, -1, BL)] + [None] * 12,
    }, {
        "KeyQuant": [IN_TOP_RIGHT, None],
    }),
    # Faces whose labels ran into a 16-W module name (rule 16): port-coordinate dials at -8.6 put
    # their two lines of text up in the name band. Dials to the bottom band, each jack tied by "-"
    # to the dial it feeds, a selector beside them at -1; CtrlSend's Send thru-output is its only
    # output, so it takes the main output's corner.
    "NameBand": ({
        "NoteQuant": [at(28, -3, BL), at(44, -3, BL)],
        "CtrlSend":  [at(28, -3, BL), at(44, -3, BL), at(60, -1, BL)],
        "NoteSend":  [at(28, -3, BL), at(44, -3, BL), at(60, -1, BL)],
        # EnvADR takes EnvADSR's face as far as three rows allow: LED and KB top-left, Gate below
        # them with its Trig/Gate selector, dials at 20 and 32, the selectors down the -24 column.
        # Params: Shape, Attack, Reset, Time, Trig/Gate, OutType, KB, Decay/Release.
        "EnvADR":    [at(-24, -3, BR), at(20, -3, BL), at(-24, -17, BR), at(32, -3, BL),
                      at(9, 13, TL), at(-24, -10, BR), at(10, 8, TL), at(44, -1, BL)],
    }, {
        "NoteQuant": [IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "CtrlSend":  [at(3, -3, BL), OUT_BOT_RIGHT, at(37, -3, BL, '"-"', "labelLocRight")],
        "NoteSend":  [at(3, -3, BL), at(21, -3, BL, '"-"', "labelLocRight"), at(37, -3, BL, '"-"', "labelLocRight")],
        # Trig/Gate, In, AM, Env, Out, End
        "EnvADR":    [at(3, 13, TL), IN_TOP_RIGHT, at(3, -3, BL, '"AM"', "labelLocRight"),
                      at(-3, -12, BR, '"Env"', "labelLocLeft"), OUT_BOT_RIGHT, at(-3, -21, BR, '"End"', "labelLocLeft")],
    }),
    # The random clocks and pattern, whose Bypass sat on Seed or an input and whose Out sat on a label:
    # one face for the four - Clk/Rst left as on the sequencers, Mode at 28 beside the dials, Step at
    # 44, StepM at 60 fed by its jack at 53, Seed at 76; down the right Bypass at the top and the Out
    # selector over the main Out it sets (CT) - unlabelled on two rows, where "Out" does not fit.
    # LevScaler's In ran off the top; Sw2-1(M) labelled In 1 up and In 2 left - left, as Sw4-1/Sw8-1.
    "Tidy": ({
        # Step, StepM, Bypass, Mode
        "RndTrig":    [at(44, -3, BL), at(60, -3, BL), at(-3, 3, TR), at(28, -1, BL)],
        # Step, Out, Bypass, Mode, StepM
        "RndClkB":    [at(44, -3, BL), at(-3, -10, BR), at(-3, 3, TR), at(28, -1, BL), at(60, -3, BL)],
        # Step, Mode, Dice, Out, Bypass - Mode and Dice print bare numbers in ~10%-wide boxes, so wider apart
        "RndClkA":    [at(48, -3, BL), at(32, -1, BL), at(20, -1, BL), at(-14, -8, BR, "NULL"), at(-3, 3, TR)],   # Out well left of Bypass/Out: under it, it clashed (CT)
        # PatA, PatB, Step, Loop, StepM, Out, Bypass. Two labelled dial rows do not fit under the name
        # band on the left of a three-row face, so PatA/PatB sit right of it (x >= 55) and higher, at -18.
        "RndPattern": [at(60, -18, BL), at(76, -18, BL), at(44, -3, BL), at(28, -3, BL), at(60, -3, BL),
                       at(-3, -10, BR), at(-3, 3, TR)],
        # L, BP, R, Kbt
        "LevScaler":  [at(28, -3, BL), at(44, -3, BL), at(60, -3, BL), at(3, -15, BL)],
        # Program, Chan - CtrlSend's face (NameBand): the value dial at 44, Chan beside it
        "PCSend":     [at(44, -3, BL), at(60, -1, BL)],
    }, {
        # Clk, Rst, Seed, Prob, Out
        "RndTrig":    [None, None, at(76, -3, BL, '"Seed"', "labelLocUp"), at(53, -3, BL, '"-"', "labelLocRight"), OUT_BOT_RIGHT],
        # Clk, Rst, Seed, Step, Out
        "RndClkB":    [None, None, at(76, -3, BL, '"Seed"', "labelLocUp"), at(53, -3, BL, '"-"', "labelLocRight"), OUT_BOT_RIGHT],
        # Clk, Rst, Seed, Out
        "RndClkA":    [None, None, at(68, -3, BL, '"Seed"', "labelLocUp"), OUT_BOT_RIGHT],
        # Clk, Rst, A, B, Step, Out
        "RndPattern": [None, None, at(53, -18, BL, '"-"', "labelLocRight"), at(69, -18, BL, '"-"', "labelLocRight"),
                       at(53, -3, BL, '"-"', "labelLocRight"), OUT_BOT_RIGHT],
        # Note, In, Level, Out
        "LevScaler":  [at(3, -3, BL, '"Note"', "labelLocRight"), IN_TOP_RIGHT, at(-17, -3, BR), OUT_BOT_RIGHT],
        # Send In, Send Out, Program
        "PCSend":     [at(3, -3, BL), OUT_BOT_RIGHT, at(37, -3, BL, '"-"', "labelLocRight")],
        # In 1, In 2, Out, Ctrl
        "Sw2-1":      [at(30, -3, BL, KEEP, "labelLocLeft"), None, None, None],
        "Sw2-1M":     [at(30, -3, BL, KEEP, "labelLocLeft"), None, None, None],
    }),
    # DXRouter: six In/Out pairs at a 14% pitch, labelled by operator, ending well clear of the main
    # output, which is unlabelled in its corner (rules 1, 10, 14 - "Main" above it ran off the edge).
    "FM": ({
        "DXRouter":  [None, None],
        # Operator, twelve rows in three bands and a foot. Oscillator: Kbt, Sync, Detune beside the
        # Coarse and Fine dials, Ratio/Fixed above Coarse. Envelope: its graph, KBEnv, Vel and
        # RateScale down the right, R1-L4 in rate/level pairs. KB level scaling: its graph over
        # left depth, BrPt, right depth - the order the graph reads. Foot: AMod beside its jack,
        # Level, Bypass above the Out. Params in the G2's order: Kbt, Sync, RatioFixed, Coarse,
        # Fine, Detune, Vel, RateScale, R1 L1 R2 L2 R3 L3 R4 L4, AMod, BrPt, LDepth Mode, LDepth,
        # RDepth Mode, RDepth, Level, Bypass, KBEnv.
        "Operator":  [at(12, 22, TL), at(22, 22, TL), at(36, 6, TL), at(36, 19, TL), at(52, 19, TL), at(68, 22, TL),
                      at(80, 54, TL), at(80, 63, TL)]
                     + [at(x, 75, TL) for x in (12, 21, 33, 42, 54, 63, 75, 84)]
                     + [at(12, -1, BL),
                        at(36, 118, TL), at(12, 120, TL), at(21, 120, TL), at(52, 120, TL), at(61, 120, TL),
                        at(76, -3, BL), at(-3, -12, BR), at(80, 45, TL)],
    }, {
        "DXRouter":  [cell for op in range(6) for cell in
                      (at(5 + 14 * op, -3, BL, '"%d"' % (op + 1), "labelLocLeft"), at(11 + 14 * op, -3, BL, "NULL"))]
                     + [at(-3, -3, BR, "NULL")],
        # Freq, FM (the main input), Gate, Note, AMod, Vel, Pitch, Out
        "Operator":  [at(3, 31, TL), at(-3, 3, TR, "NULL"), at(3, 45, TL), at(3, 57, TL),
                      at(3, -3, BL, '"--"', "labelLocRight"), at(3, 69, TL), at(3, 19, TL), OUT_BOT_RIGHT],
    }),
    # The pitch and FX modules still on port coordinates take the oscillators' pattern: the mod jack at
    # 3 tied by "--" to its dial at 12, the main dials at 28 and 44, a selector that qualifies a dial
    # directly above it at -15, anything else beside them at -1.
    "FX": ({
        "PShift":    [at(28, -3, BL), at(44, -3, BL), at(12, -3, BL, '"Shift"'), at(60, -1, BL), at(-3, 0, MR)],
        "FreqShift": [at(28, -3, BL), at(12, -3, BL), at(28, -15, BL), at(-3, 0, MR)],
        "Scratch":   [at(28, -3, BL, '"Ratio"'), at(12, -3, BL), at(44, -1, BL, '"Delay"'), at(-3, 0, MR)],
        "Digitizer": [at(44, -1, BL), at(28, -3, BL), at(12, -3, BL), at(-3, 0, MR)],
    }, {
        "PShift":    [at(3, -3, BL, '"--"', "labelLocRight"), IN_TOP_RIGHT, OUT_BOT_RIGHT],
        "FreqShift": [at(3, -3, BL, '"--"', "labelLocRight"), IN_TOP_RIGHT,
                      at(-17, -3, BR, '"Down"', "labelLocLeft"), at(-3, -3, BR, '"Up"', "labelLocLeft")],
        "Scratch":   [IN_TOP_RIGHT, at(3, -3, BL, '"--"', "labelLocRight"), OUT_BOT_RIGHT],
        "Digitizer": [IN_TOP_RIGHT, at(3, -3, BL, '"--"', "labelLocRight"), OUT_BOT_RIGHT],
    }),
}

def module_enums():
    """Module name (as the palette and the backdoor spell it) -> moduleType enum name. The names come
    from gModuleProperties and the enum from types.h; both are in type-number order."""
    res   = open(RES, encoding="latin-1").read()
    props = res[res.index("gModuleProperties[] = {"):]
    props = props[:props.index("\n};")]
    names = [m.group(1) for m in re.finditer(r'^\s*\{"([^"]*)",\s*\d+,', props, re.M)]
    types = open(TYPES, encoding="latin-1").read()
    block = types[types.index("moduleTypeUnknown0"):]
    block = block[:block.index("moduleTypeMax")]
    enums = re.findall(r"(moduleType\w+)\s*,", block)
    return {n: e for n, e in zip(names, enums) if n}

POS   = re.compile(r"\{\{\s*-?[\d.]+,\s*-?[\d.]+\}")
FIELD = re.compile(r"(anchor\w+),(\s*)(NULL|\"[^\"]*\")")
LOC   = re.compile(r"labelLoc\w+")

def apply(lines, table, spec, enum_of):
    start = next(i for i, l in enumerate(lines) if (table + "[] = {") in l)
    end   = next(i for i in range(start, len(lines)) if lines[i].rstrip().endswith("};"))
    by_enum = {enum_of[name]: name for name in spec}
    seen, changed = {}, 0
    for i in range(start, end + 1):
        if lines[i].lstrip().startswith("//"):
            continue
        m = re.match(r"\s*\{(moduleType\w+),", lines[i])
        if not m or m.group(1) not in by_enum:
            continue
        name = by_enum[m.group(1)]
        n    = seen.get(name, 0)
        seen[name] = n + 1
        if n >= len(spec[name]) or spec[name][n] is None:
            continue
        x, y, anchor, label, loc = spec[name][n]
        # Compare VALUES, not text: uncrustify pads these fields, so a row already in place must be
        # left byte-for-byte alone or every re-run would undo its alignment.
        pos = re.search(r"\{\{\s*(-?[\d.]+),\s*(-?[\d.]+)\}", lines[i])
        fld = FIELD.search(lines[i])
        cur = LOC.search(lines[i])
        if (pos and float(pos.group(1)) == x and float(pos.group(2)) == y and fld and fld.group(1) == anchor
                and (label is KEEP or fld.group(3) == label) and (loc is KEEP or (cur and cur.group(0) == loc))):
            continue
        new = POS.sub("{{%s, %s}" % (x, y), lines[i], count=1)
        new = FIELD.sub(lambda f: "%s,%s%s" % (anchor, f.group(2), f.group(3) if label is KEEP else label), new, count=1)
        if loc is not KEEP:
            new = LOC.sub(loc, new, count=1)
        if new != lines[i]:
            lines[i] = new
            changed += 1
    for name, rows in spec.items():
        if seen.get(name, 0) != len(rows):
            sys.exit("%s has %d rows in %s; the spec gives %d - nothing written"
                     % (name, seen.get(name, 0), table, len(rows)))
    return changed

def main():
    if len(sys.argv) != 2 or sys.argv[1] == "--list":
        print("families: " + ", ".join(sorted(FAMILIES)))
        return 0
    family = FAMILIES[sys.argv[1]]
    params, connectors = family[0], family[1]
    modes   = family[2] if len(family) > 2 else {}      # optional third table: modeLocationList
    enum_of = module_enums()
    missing = [n for n in list(params) + list(connectors) + list(modes) if n not in enum_of]
    if missing:
        sys.exit("not module names: " + ", ".join(missing))
    lines = open(RES, encoding="latin-1").read().split("\n")
    p = apply(lines, "paramLocationList", params, enum_of)
    c = apply(lines, "connectorLocationList", connectors, enum_of)
    m = apply(lines, "modeLocationList", modes, enum_of) if modes else 0
    open(RES, "w", encoding="latin-1").write("\n".join(lines))
    print("%s: %d parameter, %d connector and %d mode rows moved" % (sys.argv[1], p, c, m))
    return 0

if __name__ == "__main__":
    sys.exit(main())
