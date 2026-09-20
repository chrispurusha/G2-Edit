# Poly voices across cores

Living design note, started 2026-09-19. Nothing built yet. What it would take to render a patch's
voices on more than one core, what the payoff actually is, and the one thing that has to change
first.

## JUCE has nothing for the hard part

Worth saying plainly, because it was the starting assumption. `juce::Synthesiser::renderVoices()` is
this, in both the float and double overloads:

```
    for (auto* voice : voices)
        voice->renderNextBlock (buffer, startSample, numSamples);
```

A plain serial loop. Its `AudioProcessorGraph` is single-threaded too, and there is no thread pool
anywhere in its audio path. So it has no design to borrow for splitting voices - that part is ours.

**What it does have that matters** is `juce::AudioWorkgroup`, a thin wrapper over Apple's
`os_workgroup_t`, and the fact that it bothered to add one is the clue. On Apple silicon a helper
thread that has not joined the audio device's workgroup is scheduled as an ordinary thread and can
land on an efficiency core, so a naive pool makes the worst block WORSE than single-threaded. The
API is Apple's, not JUCE's, and we would call it directly:

- **Standalone:** `kAudioDevicePropertyIOThreadOSWorkgroup` (`'oswg'`, AudioHardware.h) on the
  output device gives the `os_workgroup_t`; each worker calls `os_workgroup_join()` once and
  `os_workgroup_leave()` at teardown.
- **Plug-in:** AUv3 exposes it as `AUAudioUnit.osWorkgroup`. **VST3 has no standard for it and AUv2
  is unclear - that needs checking before any of this is built**, because a plug-in whose workers
  cannot join the host's workgroup is the case where this backfires.

JUCE is AGPLv3. Read it for mechanism, never copy it - see the note in the memory on this.

## The blocker: the per-sample note grid

The render loop is

```
    for each output sample
      for each oversampled sub-step
        take_next_note_event()          <- one note event per sub-step
        for each voice
          for each node
            eval_node()
```

Note events are consumed INSIDE the sample loop, deliberately, so a chord's worth of note-ons lands
over consecutive samples rather than all at one buffer boundary (§15.3a's steal wait counts on the
same grid). That is what pins the voice loop where it is: to spread voices over threads at this
granularity would mean a fork and join every sub-step - 384,000 a second at 96 kHz - which costs
far more than the work it distributes.

**So the restructure comes first, and it is the whole job:** each voice renders a whole SUB-BLOCK
into its own buffer, the sub-block boundary becomes the finest grid a note event can land on, and
the mix plus everything postMix runs serially afterwards. A 32- or 64-sample sub-block puts note
timing at 0.7-1.3 ms, against the per-sample grid it has now - that is the cost, and it is a real
loss of timing resolution that has to be accepted deliberately.

## What is actually parallelisable, and what it buys

Measured on 02 Big Pad at 16 voices, 48 kHz, on an M4 Max (`tools/`-style offline harness, CPU
seconds per 6 s of audio):

| notes held | % of a core |
|---|---|
| 0 | 3.72 |
| 1 | 3.75 |
| 2 | 5.22 |
| 4 | 8.05 |
| 8 | 13.85 |
| 16 | 25.48 |

So about **3.7% is fixed** - the FX area (two DelayB and a Reverb), the per-sample parameter
smoothing of every node, the output decimator - and **1.36% is one voice**. With the voice loop
spread perfectly over N workers the total would be `3.7 + 21.8/N`:

| workers | % of a core | speed-up |
|---|---|---|
| 2 | 14.6 | 1.7x |
| 4 | 9.2 | 2.8x |
| 8 | 6.5 | 3.9x |

Amdahl caps it just under 7x however many cores are thrown at it, and the fork/join cost is not in
those figures. 3-4x is the realistic target.

**Check the premise before building it.** At 48 kHz this patch's worst single block is 18% of its
deadline at a 64-frame buffer, which is not a machine that should break up - and the biggest real
win so far came from not oversampling a fast device (notes §29a), which halved everything above
48 kHz. Establish where the deficit actually is - rate, buffer size, instance count - before paying
for the restructure.

## What is already in the right shape

Everything a Voice Area module remembers between samples is already indexed `[voice][node]`:
`gPhase`, `gEnvLevel` / `gEnvQ` / `gEnvTick` / `gEnvStage`, `gLadder`, `gOscHistory`, `gNoiseSeed`,
`gNoiseLp`, `gPulseCount` / `gPulsePrev`, `gCompEnv`, `gGlideOut` / `gGlidePrimed` (notes §35). Two
voices never touch the same word of it, which is the precondition and it already holds.

The shared buffers - the delay lines, the chorus lines and the reverb - are NOT per voice, and a
patch that puts one in the Voice Area gets a single shared instance. `mark_post_mix_nodes()`
already marks those, and anything downstream of one, as postMix, so they are evaluated once after
the voices are summed. **That is exactly the line the parallel/serial split wants**, and it is
already drawn.

## What has to change

1. **Note events move to sub-block boundaries.** `take_next_note_event()` and
   `start_pending_steals()` run once per sub-block instead of once per sub-step. The steal wait
   (§15.3a) is counted in envelope ticks and would need re-expressing in sub-blocks.
2. **Per-voice output buffers.** `voiceSum[node][leg]` is accumulated across voices as they are
   evaluated; each worker needs its own partial and a reduction afterwards, or each voice needs its
   own `value[][]` for the sub-block.
3. **`value[MAX_ENGINE_NODES][NODE_OUTPUTS]` becomes per worker**, not per call. It is 2 KB today.
4. **The per-sample parameter smoothing stays serial** and must run before the voices, as it does
   now - it is one knob position however many voices are sounding.
5. **A fixed pool, sized once**, with no allocation, no locks and no syscalls on the audio path:
   workers spin briefly then wait on a futex-like primitive, join the audio workgroup at creation.
6. **A serial fallback**, chosen at start-up: one voice, a host that gives us no workgroup, or a
   patch whose voice count does not repay the sync.

## Open questions

- Where the workgroup comes from in a VST3 on macOS, if anywhere. If there is none, does a plug-in
  build stay serial and only the standalone thread?
- Does a host object to a plug-in spawning its own workers? Hosts already run tracks and instances
  in parallel, so a pool per instance can oversubscribe badly - G2 Alike allows 32 instances.
- What sub-block size trades note timing against sync cost acceptably. 32 samples is 0.7 ms.
- Whether the fixed 3.7% is worth attacking first and separately: three quarters of the per-voice
  cost is `eval_node`'s switch, and the smoothing loop runs over every node every sample whether
  anything changed or not (todo.md).

## Order of work

1. Settle where the real deficit is, with CT's rate, buffer and instance count.
2. The sub-block restructure, still single-threaded, and prove it bit-identical at one voice and
   unchanged to the ear at many. This is the risky half and it stands on its own.
3. The pool and the workgroup, standalone first.
4. The plug-in, only once the workgroup question above is answered.
