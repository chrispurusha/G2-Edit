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

## 3. A zone's pitch (`zone_root_note()`)

Measured from the zone's period (autocorrelation over 4096 samples a third of the way in) until the key
map is read. Checked on the Melodica: its 16 zones come out as C#, D#, F, G, A and B over three octaves,
within about 10 cents.

## 4. Loading once (`nord_sample_get()`)

Every file a Sampler names is loaded on the first patch build that needs it and kept, keyed by its path, so
a rebuild - which happens at every knob turn - never waits on the disk again and two Samplers on one file
share it. A file that fails to load is remembered as failed. 32 files at most; the cache is never emptied.
