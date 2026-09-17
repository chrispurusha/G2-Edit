# Oscillator code before the OscShpB/OscDual edge conversion

The code the engine ran on 2026-09-17 just before OscShpB and OscDual moved to the instrument's own
edges at the engine rate (engine-law-revert-record.md rows 47-48). Parts of it had not been committed
at that point, so it is kept here verbatim: to go back, put these functions back into
`src/soundEngine.c` in place of their successors (and the notes/reference sections they cite are as they
stood in the same commit as this file). `osc_waveform()` and the basic oscillators (§6.3) are included
only to show the call path; they are not part of the revert.

```c
static double poly_blep(double t, double dt) {
    if (dt <= 0.0) {
        return 0.0;
    }

    if (t < dt) {
        t = t / dt;
        return (t + t) - (t * t) - 1.0;
    }

    if (t > (1.0 - dt)) {
        t = (t - 1.0) / dt;
        return (t * t) + (t + t) + 1.0;
    }
    return 0.0;
}

// Ramps DOWN from +1 at phase 0. OscShpB and OscDual use it as it is; the basic oscillators' saw is its
// negation half a cycle on (§6.3).
static double osc_saw(double phase, double dt) {
    return poly_blep(phase, dt) - ((2.0 * phase) - 1.0);
}

static double osc_square(double phase, double dt, double width) {
    double value = (phase < width) ? 1.0 : -1.0;

    // One correction at the rising edge (phase 0) and one at the falling edge (phase == width).
    value += poly_blep(phase, dt);
    value -= poly_blep(fmod((phase - width) + 1.0, 1.0), dt);
    return value;
}

// Symmetry-adjustable triangle: rises over the first `width` of the cycle and falls over the rest,
// so width 0.5 is the usual symmetrical shape. Not band-limited — see the header.
static double osc_triangle(double phase, double width) {
    if (phase < width) {
        return ((2.0 * phase) / width) - 1.0;
    }
    return 1.0 - ((2.0 * (phase - width)) / (1.0 - width));
}

// notes §101
#define SHP_WAVE_SINE2    (1u)
#define SINE2_DC_COEFF    (4000.0 / 8388608.0)    // §27.2 - per sample at SINE2_DC_RATE
#define SINE2_DC_RATE     (96000.0)

static double osc_shp_wave(uint32_t waveform, double phase, double dt, double shape) {
    SE_LOCAL;

    // The phase step per 96 kHz sample, which is how the instrument's Sine3/Sine4 read the pitch
    double inc96 = dt * (double)OSC_OVERSAMPLE * (gSampleRate / SINE2_DC_RATE);

    // notes §102
    switch (waveform) {
        case 0:
        {
            // The rise never shortens past two samples at this pitch, as TriSaw's fall does (notes §102)
            return wave_sine1_limited(phase, shape, 2.0 * dt * (double)OSC_OVERSAMPLE);
        }
        case 1:
        {
            // §27.2 - four samples at the least, and a gain of 1 + Shape; the DC blocker follows the decimation
            return wave_sine2_limited(phase, shape, 4.0 * dt * (double)OSC_OVERSAMPLE) * (1.0 + wave_shape_word(shape));
        }
        case 2:
        {
            return wave_sine3_instrument(phase, shape, inc96);    // §27.3
        }
        case 3:
        {
            return wave_sine4_instrument(phase, shape, inc96);
        }

        case 4:
        {
            // The fall never shortens past two samples at this pitch (dt is per oversampled sample).
            double shortest = 2.0 * dt * (double)OSC_OVERSAMPLE;

            return osc_triangle(phase, fmin(wave_trisaw_peak(shape), 1.0 - shortest));
        }
        case 5:
        {
            double second = fmod(phase + wave_dblsaw_detune(shape), 1.0);

            return osc_saw(phase, dt) + osc_saw(second, dt);    // two full saws: peak 2, as on the instrument
        }
        case 6:
        {
            // Never narrower than one sample: the instrument's two one-sample edges overlap there and
            // leave a spike rather than silence
            double duty = fmax(wave_shpb_pulse_duty(shape), dt * (double)OSC_OVERSAMPLE);

            return osc_square(phase, dt, duty) - ((2.0 * duty) - 1.0);    // with its DC taken out
        }
        default:
        {
            // SymPulse: High, then Low, then silence for the rest of the cycle. Not band-limited,
            // and deliberately so — its edges are already the two the square shares, and at Shape 1
            // the wave vanishes entirely, which is what the capture shows.
            double w = wave_sympulse_half_segment(shape);

            if (phase < w) {
// ...
// notes §151
#define OSCDUAL_SUB_SHELF_HZ      (190.0)    // §12.3
#define OSCDUAL_SUB_SHELF_LOW     (0.38)
#define OSCDUAL_SUB_SHELF_HIGH    (1.12)
#define OSCDUAL_SOFT_GAIN         (2.0)
#define OSCDUAL_SOFT_CORNER       (1.5)      // times the oscillator's pitch

// §12.3 - state: sub flip-flop, last phase, last sub square, shelf high-pass, soft low-pass, saw offset.
static double oscdual_sub(double * state, double phase, double dt, bool soft) {
    SE_LOCAL;

    double subPhase  = 0.5 * (phase + state[0]);
    double square    = osc_square(subPhase, 0.5 * dt, 0.5);
    double shelfPole = exp(-2.0 * M_PI * OSCDUAL_SUB_SHELF_HZ / (gSampleRate * (double)OSC_OVERSAMPLE));
    double highPass  = shelfPole * (state[3] + square - state[2]);
    double shelved   = (OSCDUAL_SUB_SHELF_LOW * square) + ((OSCDUAL_SUB_SHELF_HIGH - OSCDUAL_SUB_SHELF_LOW) * highPass);

    state[2]  = square;
    state[3]  = highPass;

    if (!soft) {
        return shelved;
    }
    state[4] += (1.0 - exp(-2.0 * M_PI * OSCDUAL_SOFT_CORNER * dt)) * (shelved - state[4]);
    return OSCDUAL_SOFT_GAIN * state[4];
}

// §12.2
static double oscdual_wave(uint32_t voice, uint32_t node, const tEngineNode * spec, double phase, double dt, double pulsePosition) {
    SE_LOCAL;

    double * state = gLadder[voice][node];
    double   duty  = 0.5 * (1.0 - fmin(fmax(pulsePosition, 0.0), 1.0));
    double   out   = 0.0;

    if (phase < state[1]) {
        state[0] = 1.0 - state[0];
    }
    state[1] = phase;

    if ((spec->dualSquareLevel > 0.0) && (duty > 0.0)) {
        out += spec->dualSquareLevel * (osc_square(phase, dt, duty) - ((2.0 * duty) - 1.0));
    }

    if (spec->dualSawLevel > 0.0) {
        out += spec->dualSawLevel * osc_saw(fmod(phase + state[5], 1.0), dt);
    }

    if (spec->dualSubLevel > 0.0) {
        out += spec->dualSubLevel * oscdual_sub(state, phase, dt, spec->dualSoft);
    }
    return out;
}
// ...
static double oscillator_step(uint32_t voice, uint32_t node, const tEngineNode * spec, double voicePitch,
                              double pitchDirect, double pitchVar, double shape) {
    SE_LOCAL;
    double   frequency = 0.0;
    double   dt        = 0.0;
    double   sum       = 0.0;
    uint32_t step      = 0;
    uint32_t tap       = 0;

    frequency = osc_frequency_hz(spec, voicePitch, pitchDirect, pitchVar);

    // notes §155
    if (frequency > (gSampleRate * 0.5)) {
        return 0.0;
    }
    double   inc96     = frequency / OSC_INSTRUMENT_RATE;

    // §6.3 - the basic waves are drawn for a 96 kHz sample and need no oversampling of their own
    if ((spec->kind == eNodeOsc) && (spec->wave != eOscWaveDual)) {
        dt = frequency / gSampleRate;
        return osc_waveform(voice, node, spec, advance_phase(&gPhase[voice][node], dt), dt, inc96, shape);
    }
    dt        = frequency / (gSampleRate * (double)OSC_OVERSAMPLE);

    for (step = 0; step < OSC_OVERSAMPLE; step++) {
        double phase = advance_phase(&gPhase[voice][node], dt);

        gOscHistory[voice][node][gOscHistoryPos[voice][node]] = (float)osc_waveform(voice, node, spec, phase, dt, inc96, shape);
        gOscHistoryPos[voice][node]                           = (gOscHistoryPos[voice][node] + 1) % OSC_DECIMATE_TAPS;
    }

    // notes §156
    {
        const float * history = gOscHistory[voice][node];
        uint32_t      oldest  = gOscHistoryPos[voice][node];

        for (tap = 0; tap < OSC_DECIMATE_TAPS; tap++) {
            sum += (double)history[oldest] * gOscDecimate[OSC_DECIMATE_TAPS - 1 - tap];
            oldest++;

            if (oldest >= OSC_DECIMATE_TAPS) {
                oldest = 0;
            }
        }
    }

    // §27.2 - Sine2's DC blocker, at the instrument's rate and on its own coefficient
    if ((spec->kind == eNodeOscShp) && ((uint32_t)spec->wave == SHP_WAVE_SINE2)) {
        double * state = gLadder[voice][node];
        double   a     = SINE2_DC_COEFF * (SINE2_DC_RATE / gSampleRate);
        double   held  = state[0] + (a * state[1]);

        sum      = sum - held - (2.0 * state[0]);
        state[0] = state[0] + (a * sum);
        state[1] = held;
    }
    return sum;
}

```
