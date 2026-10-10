# nordSample.c - notes

Numbered notes for `src/nordSample.c`; the code points here as `// notes §k`.

## 1. The file (`nord_sample_load()`, `chains_to_end()`)

A Nord sample file (`.nsmp`) starts `CBIN`, then `nsmp` at offset 8; from offset 0x18 it is a run of
chunks, each a three-letter tag and a zero, a type byte, a 32-bit big-endian length, then that many bytes.
Among them: `hdr` (names), `cat`, `map` (the key map), one `stk` per zone, `sty`. A zone's chunk is a
header of its own followed by coded blocks ending in a stop block. The header's size varies between file
versions, so the decoder takes the first offset from which the blocks chain exactly to a stop block at
the zone's end - the block structure is strict enough that a wrong start does not get there.

## 2. The coding (`decode_zone()`, `kPredictor`)

Each block opens with a 24-bit header: bit 23 a mode flag, bits 22-19 the residual width less one,
bit 18 a flag, bits 17-14 the predictor order (0-7), bits 13-0 the sample count. A stop block is mode 1,
width 1, order 0. The residuals follow, signed, most significant bit first, packed into 24-bit words. Each
sample is its residual plus a fixed polynomial prediction from the samples before it - the coefficients of
(1 - z^-1)^order, the predictors lossless audio coders have long used - with the history carried from block
to block. Samples are 14-bit, at 44.1 kHz.

## 3. The zone header (`decode_zone()`, `ZONE_*`)

The fields used, as offsets into a zone's chunk: the root key at 0x05 (a MIDI note), the zone's own sample
rate at 0x06 (16 bits - about 35 kHz in the files seen, which is why a zone measured at an assumed 44.1 kHz
came out four semitones sharp), and three positions, 32 bits each, at 0x12 (the zone's first block), 0x1b
(loop start) and 0x24 (loop end). A position p names the coded block that begins at file offset 0x18 + 3p -
the positions count through the whole file, not the zone. Decoding from the first block matters: started
anywhere else, the block chain still reaches the end but the opening of the attack is lost. A zone whose
header does not hold together falls back to the search of §1, 44.1 kHz, no loop, and a pitch measured
from its period (`zone_root_note()`).

Checked against a Nord Wave playing the same file: note 61 sounds C#4 (277.85 Hz) on the Wave, 277.06 Hz
here.

## 4. Loading once (`nord_sample_get()`)

Every file a Sampler names is loaded on the first patch build that needs it and kept, keyed by its path, so
a rebuild - which happens at every knob turn - never waits on the disk again and two Samplers on one file
share it. A file that fails to load is remembered as failed. 32 files at most; the cache is never emptied.

## 5. The loop (`tNordZone.looped`, `loopStart`, `loopEnd`)

The two loop positions are where their blocks begin; the encoder starts a block of raw, unpredicted samples
at each, so a player can enter there. The samples after the loop end are not a copy of the loop start but
the sound's own continuation, a few hundred samples of it, to crossfade with: on reaching the loop end the
player jumps back by the loop's length, and over the next (length - loop end) samples fades from that
continuation into the loop start (soundEngine.c `sampler_step()`). Both halves of the fade join their
neighbours exactly, so the seam does not click. Checked: a 4 s note at 61, 79 and 48 holds its level, and
its largest sample-to-sample step is within a quarter of the 99.9th percentile of its own.

