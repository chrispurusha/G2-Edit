#!/usr/bin/env python3
"""Which modules the stage patches use, against what engine-module-status.md says of each.

Needs the editor running ONLINE with the backdoor (G2_EDIT_BACKDOOR=1). Reads the User1 category from the editor's
name cache, loads each patch from its bank INTO ONE SLOT'S EDIT BUFFER (--slot, default B - whatever is there is
replaced; nothing is written to flash), lists its module types, and prints every module the patches use, most
used first, with its verdict. The ones to validate next are the most used that are not Confirmed on the G2.

    ./stage_usage.py [--slot B] [--limit N]

Run from tools/. Stdlib only.
"""
import argparse, collections, os, re, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CACHE = os.path.expanduser('~/Library/Application Support/G2-Edit/cache.txt')


def bd(*cmds):
    return subprocess.run([os.path.join(HERE, 'g2bd'), *cmds], capture_output=True, text=True).stdout


def stage_patches():
    first = open(CACHE).read().split('\n')[0]
    found = [(int(m.group(1), 16) + 1, int(m.group(2), 16) + 1, m.group(3).strip())
             for m in re.finditer(r'P([0-9A-F]{2})([0-9A-F]{2})E(.{16})', first)]
    return sorted((p for p in found if re.match(r'\d\d ', p[2])), key=lambda p: p[2])


def type_names():
    types = open(os.path.join(ROOT, 'src/types.h')).read()
    start = types.index('moduleTypeUnknown0,')
    order = re.findall(r'(moduleType\w+)', types[start:types.index('}', start)])
    number = {name: i for i, name in enumerate(order)}
    names = {}
    for m in re.finditer(r'\{palGroup\w+,\s*(moduleType\w+),\s*"([^"]+)"', open(os.path.join(ROOT, 'src/moduleResources.h')).read()):
        names[number.get(m.group(1))] = m.group(2).strip()
    return names


def verdicts():
    out = {}
    states = ('Confirmed on the G2', 'Modelled', 'Approximate', 'Partial', 'No sound', 'Not implemented')
    for line in open(os.path.join(ROOT, 'Docs/engine-module-status.md')):
        f = [x.strip() for x in line.split('|')]
        if len(f) > 3 and f[2] in states:
            out[f[1].replace(' ', '').lower()] = f[2]
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--slot', default='B'); p.add_argument('--limit', type=int, default=0)
    args = p.parse_args()
    if 'online' not in bd('COMMS'):
        sys.exit('the editor is not online - start it with G2_EDIT_BACKDOOR=1 and a G2 connected')
    patches = stage_patches()[:args.limit or None]
    names, verdict = type_names(), verdicts()
    use, where = collections.Counter(), collections.defaultdict(list)
    bd('DEVSLOT ' + args.slot)
    for bank, loc, title in patches:
        bd('BANKLOAD PATCH %d %d' % (bank, loc))
        time.sleep(5)
        types = set(int(t) for t in re.findall(r'^loc=\S+ index=\d+ type=(\d+)', bd('SLOT ' + args.slot, 'DUMP'), re.M))
        print('%-18s %3d module types' % (title, len(types)), file=sys.stderr)
        for t in types:
            n = names.get(t, 'type %d' % t)
            use[n] += 1
            where[n].append(title[:2])
    print('uses  %-22s %-20s patches' % ('module', 'verdict'))
    for n, c in use.most_common():
        v = verdict.get(n.replace(' ', '').lower(), '?')
        print('%4d  %-22s %-20s %s' % (c, n, v, ' '.join(where[n])))


if __name__ == '__main__':
    sys.exit(main())
