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

## 5. The loop (`tNordZone.looped`, `loopStart`)

The position at 0x24 is the LOOP START; the loop runs from there to the zone's last sample and is a whole
number of cycles of the note (2 to 22 in the Melodica), cut so the last sample leads straight back into the
first: across all 16 zones the seam matches to an error of 0.0001 or less. The zone plays straight through to
its end once, then round that short loop for as long as it sounds - no crossfade and no gain. The encoder
starts a block of raw, unpredicted samples there, so a player can enter at the loop start. (The position at
0x1b marks an earlier point not used here. Looping from it to 0x24 - the first reading - gave a 0.22 s loop of
a sound still drifting in pitch and level: beating at the wrap and pitch errors up to 55 cents.)

## 6. The key map (`apply_key_map()`)

The `map` chunk, in the layout of these files: a 24-bit file level and a signed 24-bit detune, three more
24-bit values, 128 per-key entries of six bytes (a level and a detune each - neutral in the files seen), one
24-bit value, then 15 bytes per zone: the zone's id (as the first word of its `stk` chunk), its level, its
detune, its TOP key, then a 16-bit value, a byte and a 16-bit value not used here. Levels are linear in units
of 2^-20 (0x100000 is 0 dB); detunes are 1/256 of a semitone. Zones are listed from the top down; each plays
from the key above the previous zone's top to its own, the lowest down to key 0 and the highest up to 127.
A map of any other size, or one that does not name every zone, is ignored and the nearest root chosen.

Checked against a Nord Wave on the Melodica: keys 48, 61, 64 and 72 sound within 5 cents of it (key 64 at
330.45 Hz against 330.46). With the nearest root instead, key 64 played the zone above (root 65 for the map's 63)
and its level wobbled 0.61 dB against the Wave's 0.16; with the map, 0.18.

## 7. Exact pitch (`NSMP_MAX_RETUNE_CENTS`)

Because the loop holds a whole number of cycles, its length over that number is the zone's true period, to a
fraction of a sample. The zone's rate is set from it so its root key sounds at exactly equal-tempered pitch;
played at the header's rate the zones sit up to 4.6 cents off. A Nord Wave also plays each zone in tune
whatever its recording's own tuning - keys 61 and 63, on zones 4.8 cents apart, both come out +4.3 - though
with a small fixed error of its own per key, up to 4.3 cents in runs of neighbouring keys. The engine is exact:
0.00 cents at 19 keys from 48 to 85. A retune of more than 50 cents is not trusted and the header rate kept.

## 8. The later layout (`tFormat`, `kFormatLater`)

A file whose fifth byte is 1 (the `.nsmp4` files for later Nord instruments) has the same chunks, zone header
and coding, laid out differently: chunks start at 0x2c and their headers are 12 bytes - the tag as four bytes
with the three letters right-aligned behind a zero, then a 32-bit version and a 32-bit length - and the coded
stream is in 32-bit words, block headers included (the fields in the same bit positions). Header positions
count 32-bit words from 0x2c: all 176 in a 44-zone file land on block starts. Checked: the 16 Violins in both
layouts play the same pitch trace at keys 60 and 72. Its key map is §10; stereo is §9.

## 9. Stereo (`tNordZone.channels`, `tFormat.wordPerChannel`)

The zone header's byte at 0x08 is its channel count, 1 or 2. A block's samples alternate between the
channels - sample i is channel i % channels - and each channel runs its own predictor history; the block's
filter order and bit width are shared. In the later layout each channel also packs its own residuals into
its own 32-bit words, and the words alternate: the channel that fills a word emits it and hands over, so a
block's data is channel 0's word 0, channel 1's word 0, channel 0's word 1, and so on - channels x
ceil((count / channels) x width / 32) words. Read as one interleaved stream, a stereo file breaks after its
first block. Checked on three Nord Grand 2 pianos (32, 36 and 17 zones): every zone chains exactly to its
end, and the two channels are separate microphones (correlation 0.2-0.5 at keys 60 and 84). Frames hold the
channels interleaved; `length` and the loop start count frames. A mono zone plays on both outputs.

Velocity layers: none of the files on hand has more than one (the pianos included - every zone record's
velocity range is 0-127).

## 10. The later key map and zone levels (`apply_key_map_later()`, `ZONE_LEVEL_DB`)

The map starts, as the original's does, with the file's level (u24, 2^20 = 0 dB), then the same 128-key table
with 10-byte entries (a level and a detune per key, unity and zero in every file seen, and four bytes not
used here), 29 bytes more, a zone count (u24) and one 16-byte record per zone: ROOT key, TOP key, BOTTOM key,
four zero bytes, a 1, the zone id (u32), a zero, a flag (1 in the Mellotrons, 0 in the pianos - not used), and
the VELOCITY range, bottom and top. A zone plays from its bottom to its top key; the ranges tile the keyboard
(Bright Piano 1: root 104 plays 102-104, root 101 plays 100-101) and the lowest reaches key 0. The top
zone is taken on up to 127. Each zone's own level is not in the map but in its header: a float at 0x39, in
dB - exactly the level the original layout's map gives the same zone (16 Violins: +0.03, +1.47, +2.38 ... dB
both ways). A zone plays at the file's level times its own.

Velocity: the engine picks the first zone whose key AND velocity ranges hold the note, so velocity layers
will choose themselves - but every file on hand has 0-127 throughout, so that part is untested.

The file's level in these files is 1.5-2 dB below the original layout's for the same library (16 Violins
+4.05 against +5.75, Flute +7.00 against +9.00), so they play that much quieter; read as written.

Not used: a second header float at 0x3e - 20.0 in every Mellotron zone, and in a piano falling steadily from
13.7 in the bottom zone to 2.5 at the top, as a piano's sustain does. Perhaps a decay applied after the sample
has run out; nothing here tests it.

