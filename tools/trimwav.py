#!/usr/bin/env python3
r"""Keep only the channels of a capture that carry data - verified sample for sample before anything is
replaced.

    trimwav.py FILE.wav --keep 4,5,18,19 [--replace]

WHY: the QU-24 presents 32 inputs and a measurement uses two or four, so a whole-desk take is mostly
bleed and the desk's own mix - a two-minute sweep was 1.3 GB. `capture --channels` avoids that for new
takes; this repairs old ones.

WHAT IT DOES
    Streams FILE in blocks and writes the kept channels, in the order given, to FILE.trim.wav: same
    rate, same 32-bit PCM, and an INFO comment naming the ORIGINAL channel numbers ("channels 4,5,18,19
    of 32 (0-indexed) ..."), because the analysis scripts address channels by number and a trimmed
    file no longer says which desk inputs it came from. It then reads both files back and compares
    every kept channel sample for sample. Only if all of them match does --replace move the trimmed
    file over the original; without --replace the original is untouched and FILE.trim.wav is left for
    inspection.

    A .json sidecar beside FILE (as measure.py writes) gains a "channels" entry recording the same.

Channel numbers are 0-INDEXED, as tools/capture has always numbered them. 32-bit PCM only - which is
what capture writes.
"""
import argparse, array, datetime, json, os, struct, sys, wave

BLOCK = 48000


def read_comment(path):
    """An existing INFO/ICMT comment after the data chunk, or ''."""
    with open(path, 'rb') as f:
        raw = f.read()
    i = raw.find(b'ICMT', 12)
    if i < 0:
        return ''
    size = struct.unpack('<I', raw[i + 4:i + 8])[0]
    return raw[i + 8:i + 8 + size].split(b'\0')[0].decode('utf-8', 'replace')


def write_header(f, rate, channels, frames, comment_bytes):
    data = frames * channels * 4
    padded = len(comment_bytes) + (len(comment_bytes) & 1)
    listbytes = 4 + 8 + padded
    f.write(b'RIFF' + struct.pack('<I', 36 + data + 8 + listbytes) + b'WAVEfmt ')
    f.write(struct.pack('<IHHIIHH', 16, 1, channels, rate, rate * channels * 4, channels * 4, 32))
    f.write(b'data' + struct.pack('<I', data))
    return listbytes, padded


def trim(path, keep, replace):
    src = wave.open(path)
    n, width, rate, frames = src.getnchannels(), src.getsampwidth(), src.getframerate(), src.getnframes()
    if width != 4:
        sys.exit(f'{path}: {8 * width}-bit samples; only 32-bit PCM is handled')
    if any(c >= n for c in keep):
        sys.exit(f'{path}: has {n} channels, cannot keep {keep}')
    if n == len(keep) and keep == list(range(n)):
        print(f'{path}: already just those channels - nothing to do')
        return
    before = read_comment(path)
    note = (f'channels {",".join(map(str, keep))} of {n} (0-indexed) from {os.path.basename(path)}, '
            f'trimmed {datetime.date.today().isoformat()}')
    if before:
        note += f'; was: {before}'
    comment = note.encode() + b'\0'
    out_path = path[:-4] + '.trim.wav'
    k = len(keep)

    with open(out_path, 'wb') as out:
        listbytes, padded = write_header(out, rate, k, frames, comment)
        done = 0
        while done < frames:
            m = min(BLOCK, frames - done)
            d = array.array('i', src.readframes(m))
            o = array.array('i', bytes(4 * m * k))
            for j, c in enumerate(keep):
                o[j::k] = d[c::n]
            out.write(o.tobytes())
            done += m
        out.write(b'LIST' + struct.pack('<I', listbytes) + b'INFOICMT' + struct.pack('<I', len(comment)) + comment)
        if padded > len(comment):
            out.write(b'\0')
    src.close()

    # VERIFY: every kept channel, every sample, against the source.
    a, b = wave.open(path), wave.open(out_path)
    if (b.getnchannels(), b.getframerate(), b.getnframes(), b.getsampwidth()) != (k, rate, frames, 4):
        sys.exit(f'{out_path}: wrong shape after writing - original left untouched')
    done = 0
    while done < frames:
        m = min(BLOCK, frames - done)
        d = array.array('i', a.readframes(m))
        e = array.array('i', b.readframes(m))
        for j, c in enumerate(keep):
            if d[c::n] != e[j::k]:
                sys.exit(f'{out_path}: channel {c} differs near frame {done} - original left untouched')
        done += m
    a.close()
    b.close()
    if read_comment(out_path) != note:
        sys.exit(f'{out_path}: comment did not read back - original left untouched')

    old, new = os.path.getsize(path), os.path.getsize(out_path)
    side = path[:-4] + '.json'
    if replace:
        os.replace(out_path, path)
        if os.path.exists(side):
            meta = json.load(open(side))
            meta['channels'] = {'kept': keep, 'of': n, 'note': note}
            json.dump(meta, open(side, 'w'), indent=2)
    print(f'{path}: {n} -> {k} channels, {old / 1e6:.1f} -> {new / 1e6:.1f} MB, all {frames} frames verified'
          + ('' if replace else f' (trimmed copy at {out_path})'))


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('files', nargs='+')
    ap.add_argument('--keep', required=True, help='0-indexed channels to keep, in order: 4,5,18,19')
    ap.add_argument('--replace', action='store_true', help='replace each original once verified')
    args = ap.parse_args()
    keep = [int(x) for x in args.keep.split(',')]
    for f in args.files:
        trim(f, keep, args.replace)
