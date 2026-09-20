/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
// Notes: Docs/code-notes/paramCurves.c.md - "// notes §k" refers there.

// notes §1

#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "paramCurves.h"

// The oscillator dial curves, kept apart from the renderers that print them so the sound engine
// can share them. See renderParams.h for why. Nothing here touches global state, so both the
// UI thread and the audio thread's parameter snapshot can call them freely.

int osc_pitch_type_param_index(tModule * module) {
    switch (module->type) {
        case moduleTypeOscB:
        case moduleTypeResonator:
        case moduleTypeOscShpB:
        case moduleTypeOscString:
        case moduleTypeOscNoise:
        case moduleTypeOscShpA:
        case moduleTypeOscDual:
        {
            return 4;
        }
        case moduleTypeOscMaster:
        case moduleTypeOscC:
        case moduleTypeOscPM:
        {
            return 3;
        }
        case moduleTypeOscPerc:
        {
            return 2;
        }
        case moduleTypeOscA:
        {
            return 6;
        }
        default:
        {
            return -1;
        }
    }
}

double osc_freq_semitones(double paramValue) {
    if (paramValue >= 127.0) {
        return 63.0;    // Clip - the dial's top step is +63, not +64
    }
    return paramValue - 64.0;
}

double osc_freq_hz(double paramValue) {
    double minFreq = 8.1758;    // A concert-pitch C-1
    double maxFreq = 12550.0;

    return exp(paramValue / 127.0 * log(maxFreq / minFreq)) * minFreq;
}

double osc_freq_factor(double paramValue) {
    double minFactor = 0.0248;
    double maxFactor = 38.072;

    return exp(paramValue / 127.0 * log(maxFactor / minFactor)) * minFactor;
}

double osc_fine_cents(double paramValue) {
    return (paramValue - 64.0) / 64.0 * 50.0;
}

// notes §2
#define OSC_SUB_OCTAVES_DOWN    (11.0)

double osc_sub_freq_hz(double paramValue, double fineSemitones) {
    if (paramValue <= 0.0) {
        return 0.0;
    }
    return 440.0 * exp2(((paramValue + fineSemitones - 69.0) / 12.0) - OSC_SUB_OCTAVES_DOWN);
}

double osc_shape_percent(double paramValue) {
    return paramValue * 49.0 / 127.0 + 50.0;
}

// notes §3
double lfo_shape_percent(double paramValue) {
    return paramValue * 98.0 / 127.0 + 1.0;
}

// 13.75 Hz is A-1, so like the oscillators' Tune the filter's Freq dial is really a pitch — its
// value counts semitones up from there, reaching about 21 kHz at 127 (manual: "13.76 Hz to 21.1 kHz").
double flt_cutoff_hz(double paramValue) {
    // exp2 rather than pow(2, x): the same value, but this is called once per filter per voice
    // per oversampled sample, where a general pow() is several times the cost of the base-2 form.
    return 13.75 * exp2(paramValue / 12.0);
}

// notes §4
#define ADR_TIME_OFFSET    (40.167)
#define ADR_TIME_MAX_S     (45.0)      // The manual's stated maximum, and what raw 127 must give
#define ADR_TIME_SCALE     (ADR_TIME_MAX_S / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET) \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET)                \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET)                \
                            / (127.0 + ADR_TIME_OFFSET) / (127.0 + ADR_TIME_OFFSET))

double adr_time_seconds(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return ADR_TIME_SCALE * pow(value + ADR_TIME_OFFSET, 8.0);
}

// notes §5

double env_attack_level(uint32_t envShape, double progress) {
    switch (envShape) {
        case eEnvShapeLogExp:
        {
            // Log - rises fast, then flattens towards the peak.
            return (1.0 - exp(-ENV_RISE_SHARPNESS * progress)) / (1.0 - exp(-ENV_RISE_SHARPNESS));
        }
        case eEnvShapeExpExp:
        {
            // Exp - starts slowly, then accelerates into the peak.
            return (exp(ENV_RISE_SHARPNESS * progress) - 1.0) / (exp(ENV_RISE_SHARPNESS) - 1.0);
        }
        default:
        {
            return progress;   // LinExp and LinLin both rise in a straight line
        }
    }
}

// The falling segments - decay and release - are exponential for every shape except LinLin, whose
// second word is the one that says so.
double env_fall_level(uint32_t envShape, double progress) {
    if (envShape == (uint32_t)eEnvShapeLinLin) {
        return 1.0 - progress;
    }
    return (exp(-ENV_FALL_SHARPNESS * progress) - exp(-ENV_FALL_SHARPNESS))
           / (1.0 - exp(-ENV_FALL_SHARPNESS));
}

// notes §6
#define FLT_RESONANCE_DAMPING_SPAN    (0.9)

// notes §7
#define FLT_LADDER_K_MAX              (4.0)

double flt_resonance_q(double paramValue) {
    double value = paramValue;
    double damping;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    damping = 1.0 - (FLT_RESONANCE_DAMPING_SPAN * value / 127.0);
    return 0.5 / (damping * damping);
}

// notes §8
double flt_ladder_feedback(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return FLT_LADDER_K_MAX * value / 127.0;
}

// Which stage the output is tapped from: 2, 3 or 4 poles for 12, 18 or 24 dB per octave. The loop
// length is NOT this — see flt_ladder_feedback() — it is always four.
uint32_t flt_ladder_tap(uint32_t slopeValue) {
    return 2 + flt_slope_extra_poles(slopeValue);
}

// notes §9
double flt_ladder_magnitude(double ratio, double feedback, uint32_t tap) {
    double onePoleMag   = 1.0 / sqrt(1.0 + (ratio * ratio));
    double onePolePhase = -atan(ratio);
    double loopMag      = onePoleMag * onePoleMag * onePoleMag * onePoleMag;  // |G|^4
    double loopPhase    = 4.0 * onePolePhase;
    double denomReal    = 1.0 + (feedback * loopMag * cos(loopPhase));
    double denomImag    = feedback * loopMag * sin(loopPhase);
    double denom        = sqrt((denomReal * denomReal) + (denomImag * denomImag));
    double numerator    = pow(onePoleMag, (double)tap);

    // At the self-oscillation threshold the denominator goes to zero and the magnitude to infinity.
    // A drawn curve has to stay on the page, so the floor here is what stops a peak at maximum Res
    // from becoming a vertical line; it sits far above anything the box can show.
    if (denom < 1e-4) {
        denom = 1e-4;
    }
    return numerator / denom;
}

// notes §10

// notes §11
uint32_t flt_cascade_poles(uint32_t slopeMode) {
    return (slopeMode > 5u) ? 6u : (slopeMode + 1u);
}

double flt_cascade_magnitude(double ratio, uint32_t poles, bool highPass) {
    double   onePole = 1.0 / sqrt(1.0 + (ratio * ratio));
    double   stage   = highPass ? (ratio * onePole) : onePole; // |jw/(1+jw)| against |1/(1+jw)|
    double   out     = 1.0;
    uint32_t i       = 0;

    for (i = 0; i < poles; i++) {
        out *= stage;
    }

    return out;
}

// notes §12
double flt_static_q(double paramValue) {
    double value   = paramValue;
    double damping = 0.0;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 126.0) {
        value = 126.0;      // one step short of zero damping, so the curve stays finite
    }
    damping = 1.0 - (value / 128.0);
    return 0.5 / (damping * damping);
}

// A two-pole section at f/fc = ratio, as low-pass, band-pass or high-pass. Written in real
// arithmetic for the same reason flt_ladder_magnitude() is - no complex.h in either caller.
double flt_biquad_magnitude(double ratio, double q, tFilterShape shape) {
    double r2    = ratio * ratio;
    double real  = 1.0 - r2;
    double imag  = ratio / q;
    double denom = sqrt((real * real) + (imag * imag));
    double num   = 1.0;

    switch (shape) {
        case eFilterShapeHighPass:
        {
            num = r2;
            break;
        }
        case eFilterShapeBandPass:
        {
            num = ratio / q;
            break;
        }
        case eFilterShapeBandReject:
        {
            num = fabs(1.0 - r2);
            break;
        }
        default:
        {
            num = 1.0;      // low-pass
            break;
        }
    }
    return num / denom;
}

// notes §13
double flt_nord_gc_gain(double resParam) {
    double v  = resParam / 127.0;
    double dB = (-10.897 * v) + (13.140 * v * v) - (26.503 * v * v * v);

    return pow(10.0, dB / 20.0);
}

// notes §14
uint32_t flt_nord_tap(uint32_t dbOctValue) {
    return (dbOctValue == 0u) ? 2u : 4u;    // 12dB taps two poles, 24dB taps four
}

// The dB scroll button selects how many one-pole stages sit on top of the base two: 12, 18 or 24 dB
// per octave.
uint32_t flt_slope_extra_poles(uint32_t slopeValue) {
    return (slopeValue > 2) ? 2 : slopeValue;
}

// The Kbt scroll button is Off / 25% / 50% / 75% / 100% keyboard tracking (manual p.196).
double flt_kbt_amount(uint32_t kbtValue) {
    return (kbtValue > 4) ? 1.0 : ((double)kbtValue * 0.25);
}

// notes §15
#define LFO_SEMITONE_RATIO       (1.0 / 12.0)
#define LFO_HI_BASE_HZ           (0.2555)   // Rate Hi at dial 0; Rate Lo is this over 16
// The delay's clocked time uses the same fixed tempo, for the same reason - see ENGINE_REFERENCE_BPM.
#define LFO_CLK_REFERENCE_BPM    (120.0)

double lfo_rate_hz(uint32_t rangeMode, double paramValue) {
    switch (rangeMode) {
        case 0:   // Rate Sub: a period of 699 s down to 5.46 s
        {
            // notes §16
            return (paramValue + 1.0) / 699.0506666667;
        }
        case 1:   // Rate Lo: 0.01597 Hz (62.6 s/cycle) to 24.5 Hz
        {
            // notes §17
            return (LFO_HI_BASE_HZ / 16.0) * exp2(paramValue * LFO_SEMITONE_RATIO);
        }
        case 2:   // Rate Hi: 0.2555 Hz to 392 Hz
        {
            // MEASURED EXACT: 0.25553 Hz at dial 0 and 4.08794 Hz at 48, against 0.2555 and 4.0880
            // predicted - 0.01% and 0.001%. This constant needed no correction.
            return LFO_HI_BASE_HZ * exp2(paramValue * LFO_SEMITONE_RATIO);
        }
        case 3:   // BPM: three straight runs, 24..214, always a whole number of beats
        {
            // Confirmed unchanged against the instrument's own three-branch arithmetic, including
            // the two joins: the branches meet at 88 and at 152 whichever side of the boundary they
            // are taken from, so the one-step difference in where this splits changes no value.
            double bpm = (paramValue < 33.0) ? (24.0 + round(2.0 * paramValue))
                         : (paramValue < 97.0) ? (56.0 + round(paramValue))
                         : (154.0 + round(2.0 * (paramValue - 97.0)));

            return bpm / 60.0;
        }
        case 4:   // Rate Clk: the master clock divided - §28
        {
            // The instrument's LFO sync ratios (its own 32-entry table) are 256/beats, which is
            // EXACTLY the series clk_sync_beats() already holds for the delay's Clk, entry for entry.
            // One table serves both, and the delay's reference tempo serves both too - the engine has
            // no live master clock yet, so like the delay this runs at LFO_CLK_REFERENCE_BPM.
            double beats = clk_sync_beats(paramValue);

            return (beats > 0.0) ? ((LFO_CLK_REFERENCE_BPM / 60.0) / beats) : 1.0;
        }
        default:
        {
            return 1.0;
        }
    }
}

#define DX_LEVEL_MAX    (127.0)   // the G2 reads 127 at the top (panel, 2026-08-10) - not the DX's 99

static double operator_env_level(uint32_t value) {
    return fmin((double)value, DX_LEVEL_MAX) / DX_LEVEL_MAX;
}

static double operator_env_width(uint32_t rate, double from, double to) {
    double slowness = (DX_LEVEL_MAX - fmin((double)rate, DX_LEVEL_MAX)) / DX_LEVEL_MAX;

    return fabs(to - from) * (0.03 + (0.25 * slowness));
}

// §17.9 - THE STAGE MAP OF EVERY ENVELOPE MODULE, in one place because two things need it: the face
// draws these and the engine plays them. Stages carry which PARAMETER sets each time and level, not
// the times themselves - `width` is a drawing width and nothing else (a real envelope spans 0.5 ms to
// 45 s and will not draw to scale in a box), so a player reads timeParam through env_time_seconds()
// and ignores it.
// notes §61
static double env_graph_time_width(const tParam * p, uint32_t index) {
    return 0.04 + (((double)p[index].value / 127.0) * 0.20);
}

static double env_graph_level(const tParam * p, uint32_t index) {
    return (double)p[index].value / 127.0;
}

static void env_graph_add(tEnvGraph * graph, double width, double level, bool sustain, int32_t timeParam, int32_t levelParam) {
    if (graph->count < ENV_GRAPH_MAX_SEGMENTS) {
        graph->segment[graph->count++] = (tEnvGraphSegment){
            width, level, sustain, timeParam, levelParam
        };
    }
}


// notes §86

// notes §83. Reads a parameter array rather than the module, so a handle can try a value on a copy.
bool env_stage_map(tModuleType type, const tParam * p, tEnvGraph * graph, bool applyOutputType) {
    *graph = (tEnvGraph){
        .shape = eEnvShapeLinExp
    };

    switch (type) {
        case moduleTypeEnvADSR:
        case moduleTypeModADSR:
        {
            bool    mod = (type == moduleTypeModADSR);
            int32_t a   = mod ? 0 : 1;           // A, D, S, R run in order from here

            graph->shape      = mod ? (uint32_t)eEnvShapeLinExp : p[0].value;
            graph->outputType = p[mod ? 8 : 5].value;
            env_graph_add(graph, env_graph_time_width(p, a), 1.0, false, a, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, a + 1), env_graph_level(p, a + 2), false, a + 1, a + 2);
            env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, env_graph_level(p, a + 2), true, ENV_NO_PARAM, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, a + 3), 0.0, false, a + 3, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvADR:
        {
            graph->shape      = p[0].value;
            graph->outputType = p[5].value;
            env_graph_add(graph, env_graph_time_width(p, 1), 1.0, false, 1, ENV_NO_PARAM);

            if ((p[7].value == 1) && (p[4].value == 1)) {   // Release mode, and gated - the manual's ASR
                env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, 1.0, true, ENV_NO_PARAM, ENV_NO_PARAM);
            }
            env_graph_add(graph, env_graph_time_width(p, 3), 0.0, false, 3, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvAHD:
        case moduleTypeModAHD:
        {
            bool    mod = (type == moduleTypeModAHD);
            int32_t a   = mod ? 0 : 1;           // A, H, then D two further on in EnvAHD (Reset sits between)
            int32_t d   = mod ? 2 : 4;

            graph->shape      = mod ? (uint32_t)eEnvShapeLinExp : p[0].value;
            graph->outputType = p[mod ? 6 : 5].value;
            env_graph_add(graph, env_graph_time_width(p, a), 1.0, false, a, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, a + 1), 1.0, false, a + 1, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, d), 0.0, false, d, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvD:
        {
            graph->outputType = p[1].value;
            env_graph_add(graph, 0.0, 1.0, false, ENV_NO_PARAM, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, 0), 0.0, false, 0, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvH:
        {
            graph->outputType = p[1].value;
            env_graph_add(graph, 0.0, 1.0, false, ENV_NO_PARAM, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, 0), 1.0, false, 0, ENV_NO_PARAM);
            env_graph_add(graph, 0.0, 0.0, false, ENV_NO_PARAM, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvADDSR:
        {
            bool sustainAtL1 = (p[8].value == 0);

            graph->shape      = p[1].value;
            graph->outputType = p[9].value;
            env_graph_add(graph, env_graph_time_width(p, 2), 1.0, false, 2, ENV_NO_PARAM);
            env_graph_add(graph, env_graph_time_width(p, 3), env_graph_level(p, 4), false, 3, 4);

            if (sustainAtL1) {
                env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, env_graph_level(p, 4), true, ENV_NO_PARAM, ENV_NO_PARAM);
            }
            env_graph_add(graph, env_graph_time_width(p, 5), env_graph_level(p, 6), false, 5, 6);

            if (sustainAtL1 == false) {
                env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, env_graph_level(p, 6), true, ENV_NO_PARAM, ENV_NO_PARAM);
            }
            env_graph_add(graph, env_graph_time_width(p, 7), 0.0, false, 7, ENV_NO_PARAM);
            break;
        }
        case moduleTypeEnvMulti:
        {
            // L1-L4 are params 0-3 and T1-T4 4-7; Sustain (9) is L1, L2, L3 or none.
            bool bip = (p[10].value == 4);

            graph->shape         = p[12].value;
            graph->outputType    = p[10].value;
            graph->bipolarLevels = bip;

            for (int32_t stage = 0; stage < 4; stage++) {
                double level = bip ? (((double)p[stage].value - 64.0) / 64.0) : env_graph_level(p, stage);

                if (stage == 0) {
                    graph->startLevel = 0.0;
                }
                env_graph_add(graph, env_graph_time_width(p, 4 + stage), level, false, 4 + stage, stage);

                if ((stage < 3) && (p[9].value == (uint32_t)stage)) {
                    env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, level, true, ENV_NO_PARAM, ENV_NO_PARAM);
                }
            }

            if (p[8].value == 0) {             // Normal: a retrigger starts from where L4 left it
                graph->startLevel = graph->segment[graph->count - 1].level;
            }
            break;
        }
        case moduleTypeOperator:
        {
            // notes §86: from L4 through L1, L2 and L3 (held) and back to L4. R1-R4 are params 8, 10,
            // 12 and 14; L1-L4 are 9, 11, 13 and 15.
            double from = operator_env_level(p[15].value);

            graph->startLevel = from;

            for (int32_t stage = 0; stage < 4; stage++) {
                double to = operator_env_level(p[9 + (2 * stage)].value);

                env_graph_add(graph, operator_env_width(p[8 + (2 * stage)].value, from, to), to, false, 8 + (2 * stage), 9 + (2 * stage));

                if (stage == 2) {
                    env_graph_add(graph, ENV_GRAPH_SUSTAIN_WIDTH, to, true, ENV_NO_PARAM, ENV_NO_PARAM);
                }
                from = to;
            }

            break;
        }
        default:
            return false;
    }

    // Bip and BipInv hold their sustain at the centre and end at the far extreme (manual). This
    // rewrites the LEVELS into what comes out, which is what a picture of the output wants - the
    // engine applies the output type itself (§17.6) and asks for the stages untouched.
    if (  (applyOutputType == true)
       && ((graph->outputType == 4) || (graph->outputType == 5)) && (graph->bipolarLevels == false)) {
        for (uint32_t i = 0; i < graph->count; i++) {
            if (graph->segment[i].sustain) {
                graph->segment[i].level = 0.0;

                if (i > 0) {
                    graph->segment[i - 1].level = 0.0;
                }
            }
        }

        graph->segment[graph->count - 1].level = -1.0;
    }
    return true;
}

// notes §18
int delay_time_clk_param_index(tModuleType moduleType) {
    switch (moduleType) {
        case moduleTypeDelayB:
        {
            return 4;
        }
        case moduleTypeDelayA:
        {
            return 5;
        }
        case moduleTypeDlyStereo:
        {
            return 6;
        }
        case moduleTypeDelayQuad:
        {
            return 8;
        }
        default:
        {
            return -1;
        }
    }
}

// notes §19
double delay_range_max_seconds(tModuleType moduleType, uint32_t rangeValue) {
    static const double sevenWay[]  = {0.005, 0.025, 0.100, 0.500, 1.0, 2.0, 2.7};
    static const double abWay[]     = {0.500, 1.0, 2.0, 2.7};
    static const double stereoWay[] = {0.500, 1.0, 1.35};
    const double *      table       = sevenWay;
    uint32_t            count       = sizeof(sevenWay) / sizeof(sevenWay[0]);

    switch (moduleType) {
        case moduleTypeDelayA:
        case moduleTypeDelayB:
        {
            table = abWay;
            count = sizeof(abWay) / sizeof(abWay[0]);
            break;
        }
        case moduleTypeDlyStereo:
        {
            table = stereoWay;
            count = sizeof(stereoWay) / sizeof(stereoWay[0]);
            break;
        }
        default:
        {
            break;
        }
    }
    return table[(rangeValue < count) ? rangeValue : (count - 1)];
}

// notes §20
double delay_time_seconds(double maxSeconds, double paramValue) {
    double step = round((maxSeconds * G2_ENGINE_SAMPLE_RATE) / 127.0);

    return ((paramValue * step) + 1.0) / G2_ENGINE_SAMPLE_RATE;
}

// notes §21
static const uint8_t kClkSyncSlot[32] = {
    31, 30, 29, 28, 27, 26, 25, 24, 24, 23, 23, 22, 22, 21, 21, 20,
    20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 13, 12, 11, 10
};

uint32_t clk_sync_index(double paramValue) {
    int value = (int)paramValue;

    if (value < 0) {
        value = 0;
    } else if (value > 127) {
        value = 127;
    }
    return kClkSyncSlot[value >> 2];
}

// That division as a multiple of one beat. "1/4" IS the beat, so it is 1.0; D is dotted (x1.5) and
// T is a triplet (x2/3). Same entries clkSyncStrMap prints, so heard and shown cannot diverge.
double clk_sync_beats(double paramValue) {
    static const double beats[32] = {
        256.0,     192.0,     128.0,  96.0,      64.0,       48.0,      32.0,      24.0,
        16.0,       12.0,       8.0,   6.0,       4.0,        3.0, 8.0 / 3.0,       2.0,
        1.5,   4.0 / 3.0,       1.0,  0.75, 2.0 / 3.0,        0.5,     0.375, 1.0 / 3.0,
        0.25,     0.1875, 1.0 / 6.0, 0.125,   0.09375, 1.0 / 12.0,    0.0625, 1.0 / 24.0
    };

    return beats[clk_sync_index(paramValue)];
}

// notes §22

// PShift's Semi: a QUARTER of a semitone per dial step, so the whole dial spans -16.0 to +15.75
// rather than the ±64 its name suggests. Read -16.0 / +0.0 / +15.8 at raw 0 / 64 / 127, and that
// last one says the top step is NOT rounded up to +16.
#define PSHIFT_SEMI_STEPS_PER_SEMITONE    (4.0)

double pshift_semitones(double paramValue) {
    return (paramValue - 64.0) / PSHIFT_SEMI_STEPS_PER_SEMITONE;
}

// notes §23
#define SCRATCH_STEPS_PER_MULTIPLE    (16.0)
#define SCRATCH_MAX_MULTIPLE          (4.0)

double scratch_ratio(double paramValue) {
    if (paramValue >= 127.0) {
        return SCRATCH_MAX_MULTIPLE;
    }
    return (paramValue - 64.0) / SCRATCH_STEPS_PER_MULTIPLE;
}

// The Digitizer's Sample Rate, in hertz. A PITCH SCALE, not a linear rate: twelve dial steps to a
// doubling, from 32.70 Hz - which is C1 - up to 50.2 kHz. Read 32.70 Hz / 1.32 kHz / 50.2 kHz at
// raw 0 / 64 / 127, all three matching to the printed precision.
double digitizer_rate_hz(double paramValue) {
    return 1760.0 * exp2((paramValue - 69.0) / 12.0);
}

// notes §24
double pitchtrack_threshold_db(double paramValue) {
    if (paramValue <= 0.0) {
        return -1.0 / 0.0;    // Silence - the panel shows this step as "- Infinity"
    }
    return 20.0 * log10(paramValue / 127.0);
}

// notes §25
#define FLANGER_RATE_STEP    (384000.0 / 16777216.0)

double flanger_rate_hz(double paramValue) {
    if (paramValue <= 0.0) {
        return FLANGER_RATE_STEP / 2.0;
    }
    return paramValue * FLANGER_RATE_STEP;
}

// notes §26
#define PHASER_RATE_STEP      (24000.0 / 16777216.0)
#define PHASER_RATE_OFFSET    (768000.0 / 16777216.0)

double phaser_rate_hz(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return (floor((value * value) / 2.0) * PHASER_RATE_STEP) + PHASER_RATE_OFFSET;
}

// notes §27
#define MIX_LEVEL_CUBIC_MIX    (0.99)

double mix_level_db(double paramValue) {
    double gain = mix_level_gain(paramValue);

    if (gain <= 0.0) {
        return -1.0 / 0.0;    // Silence - the dial's bottom step, shown as "-oo"
    }
    return 20.0 * log10(gain);
}

// The same curve as an amplitude (sound engine reference §3.2).
double mix_level_gain(double paramValue) {
    double x = paramValue / 127.0;

    if (x <= 0.0) {
        return 0.0;
    }

    if (x > 1.0) {
        x = 1.0;
    }
    return (x * (1.0 - MIX_LEVEL_CUBIC_MIX)) + (x * x * x * MIX_LEVEL_CUBIC_MIX);
}

// notes §28
#define PATCH_VOLUME_BASE    (16.0 / 3.0)
#define PATCH_VOLUME_SPAN    (18.0)

double patch_volume_db(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return -((pow(PATCH_VOLUME_BASE, (127.0 - value) / 127.0) * PATCH_VOLUME_SPAN) - PATCH_VOLUME_SPAN);
}

// notes §45
#define VIBRATO_TICK_HZ        (96000.0 / 94.0)   // the instrument's pitch tick; glide steps at it too
#define VIBRATO_PHASE_STEPS    (65536.0)          // one cycle of the vibrato's phase

double vibrato_rate_hz(double paramValue) {
    double value = paramValue;

    if (value < 0.0) {
        value = 0.0;
    } else if (value > 127.0) {
        value = 127.0;
    }
    return (255.0 + (256.0 * value / 127.0)) * VIBRATO_TICK_HZ / VIBRATO_PHASE_STEPS;
}

// notes §29
#define LEV_AMP_LINEAR_TOP    (24.0)       // dial position where the linear bottom segment ends
#define LEV_AMP_UNITY         (64.0)       // and where the multiplier passes through 1.0

double lev_amp_gain(double paramValue) {
    double value = paramValue;

    if (value <= 0.0) {
        return 0.0;    // Fully closed - measured 64 dB down, which is the noise floor, not a level
    }

    if (value >= 127.0) {
        return 4.0;
    }

    if (value <= LEV_AMP_LINEAR_TOP) {
        return value / 96.0;
    }

    if (value <= LEV_AMP_UNITY) {
        return 0.25 * exp2((value - LEV_AMP_LINEAR_TOP) / 20.0);
    }

    if (value <= 96.0) {
        return exp2((value - LEV_AMP_UNITY) / 32.0);
    }
    return 2.0 * exp2((value - 96.0) / 31.0);
}

// notes §44
double constant_level(double paramValue, bool bipolar) {
    if (paramValue >= 127.0) {
        return 1.0;
    }

    if (bipolar == true) {
        return (paramValue - 64.0) / 64.0;
    }
    return (paramValue <= 0.0) ? 0.0 : (paramValue / 128.0);
}

// notes §30
#define CLIP_PARAM_LEVEL_MOD       (0)
#define CLIP_PARAM_LEVEL           (1)
#define CLIP_PARAM_SHAPE           (2)
#define CLIP_PARAM_ACTIVE          (3)

#define OD_PARAM_AMOUNT_MOD        (0)
#define OD_PARAM_AMOUNT            (1)
#define OD_PARAM_ACTIVE            (2)
#define OD_PARAM_TYPE              (3)
#define OD_PARAM_SHAPE             (4)

#define SAT_PARAM_AMOUNT           (0)
#define SAT_PARAM_AMOUNT_MOD       (1)
#define SAT_PARAM_ACTIVE           (2)
#define SAT_PARAM_CURVE            (3)

#define SHPEXP_PARAM_AMOUNT        (0)
#define SHPEXP_PARAM_AMOUNT_MOD    (1)
#define SHPEXP_PARAM_ACTIVE        (2)
#define SHPEXP_PARAM_CURVE         (3)

#define WRAP_PARAM_AMOUNT_MOD      (0)
#define WRAP_PARAM_AMOUNT          (1)
#define WRAP_PARAM_ACTIVE          (2)

#define SHPSTATIC_PARAM_MODE       (0)
#define SHPSTATIC_PARAM_ACTIVE     (1)

#define RECT_PARAM_MODE            (0)
#define RECT_PARAM_ACTIVE          (1)

// A level dial as the instrument reads it: over 128, with its top step reaching exactly 1.
static double shaper_dial_fraction(double value) {
    return (value >= 127.0) ? 1.0 : (value / 128.0);
}

bool shaper_settings_build(tModule * module, uint32_t variation, tParamReader dial, tShaperSettings * out) {
    // Where a module has no dial at all - ShpStatic and Rect are pure mode selectors - the amount
    // stays at full and nothing reads it.
    *out = (tShaperSettings){
        .kind = eShaperRect, .curve = 0, .sym = true, .amount = 1.0, .mod = 0.0, .signalLeg = 0, .active = true
    };

    switch (module->type) {
        case moduleTypeClip:
        {
            out->kind   = eShaperClip;
            out->amount = dial(module, variation, CLIP_PARAM_LEVEL) / 128.0;   // notes §36: 127 leaves 1/128
            out->mod    = shaper_dial_fraction(dial(module, variation, CLIP_PARAM_LEVEL_MOD));
            out->sym    = (module->param[variation][CLIP_PARAM_SHAPE].value != 0);
            out->active = (dial(module, variation, CLIP_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeOverdrive:
        {
            out->kind   = eShaperOverdrive;
            out->amount = dial(module, variation, OD_PARAM_AMOUNT) / 127.0;
            out->mod    = dial(module, variation, OD_PARAM_AMOUNT_MOD) / 127.0;
            out->curve  = module->param[variation][OD_PARAM_TYPE].value;
            out->sym    = (module->param[variation][OD_PARAM_SHAPE].value != 0);
            out->active = (dial(module, variation, OD_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeSaturate:
        {
            out->kind   = eShaperSaturate;
            out->amount = shaper_dial_fraction(dial(module, variation, SAT_PARAM_AMOUNT));
            out->mod    = shaper_dial_fraction(dial(module, variation, SAT_PARAM_AMOUNT_MOD));
            out->curve  = module->param[variation][SAT_PARAM_CURVE].value;
            out->active = (dial(module, variation, SAT_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeShpExp:
        {
            out->kind   = eShaperShpExp;
            out->amount = shaper_dial_fraction(dial(module, variation, SHPEXP_PARAM_AMOUNT));
            out->mod    = shaper_dial_fraction(dial(module, variation, SHPEXP_PARAM_AMOUNT_MOD));
            out->curve  = module->param[variation][SHPEXP_PARAM_CURVE].value;
            out->active = (dial(module, variation, SHPEXP_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeWaveWrap:
        {
            // THE ONLY SHAPER WHOSE MOD JACK COMES FIRST, so its signal is on leg 1.
            out->kind      = eShaperWaveWrap;
            out->amount    = dial(module, variation, WRAP_PARAM_AMOUNT) / 127.0;
            out->mod       = dial(module, variation, WRAP_PARAM_AMOUNT_MOD) / 127.0;
            out->signalLeg = 1;
            out->active    = (dial(module, variation, WRAP_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeShpStatic:
        {
            out->kind   = eShaperShpStatic;
            out->curve  = module->param[variation][SHPSTATIC_PARAM_MODE].value;
            out->active = (dial(module, variation, SHPSTATIC_PARAM_ACTIVE) != 0.0);
            return true;
        }
        case moduleTypeRect:
        {
            out->curve  = module->param[variation][RECT_PARAM_MODE].value;
            out->active = (dial(module, variation, RECT_PARAM_ACTIVE) != 0.0);
            return true;
        }
        default:
            return false;
    }
}

// Fold rather than clip: a triangle of period 4 that runs straight through [-1, 1] and turns back
// on itself outside it, so 1.5 comes back as 0.5 and 3.0 as -1.0. This is what makes WaveWrap
// generate its own overtones instead of the clipped ones a limiter would.
static double shaper_fold(double x) {
    double y = fmod(x + 1.0, 4.0);

    if (y < 0.0) {
        y += 4.0;
    }
    return (y <= 2.0) ? (y - 1.0) : (3.0 - y);
}

// notes §46 - the instrument's signals saturate at four times full scale, not at it.
#define SHAPER_HEADROOM    (4.0)

static double shaper_limit(double x, double limit) {
    if (x > limit) {
        return limit;
    }

    if (x < -limit) {
        return -limit;
    }
    return x;
}

double shaper_transfer(const tShaperSettings * settings, double amount, double input) {
    double x = shaper_limit(input, SHAPER_HEADROOM);
    double s = fabs(x);

    if (amount < 0.0) {
        amount = 0.0;
    } else if (amount > 1.0) {
        amount = 1.0;
    }

    switch (settings->kind) {
        case eShaperRect:
        {
            // Exact, from the manual: discard negatives, discard positives, mirror negatives up,
            // mirror positives down. rectStrMap is {HalfPos, HalfNeg, FullPos, FullNeg}.
            switch (settings->curve) {
                case 0:  return (x > 0.0) ? x : 0.0;

                case 1:  return (x < 0.0) ? x : 0.0;

                case 2:  return fabs(x);

                default: return -fabs(x);
            }
        }
        case eShaperShpStatic:
        {
            // notes §31
            double y = 0.0;

            switch (settings->curve) {
                case 0:  y = (s >= 1.0) ? 1.0 : (1.0 - ((1.0 - s) * (1.0 - s) * (1.0 - s))); // Inv x3
                    break;

                case 1:  y = (s >= 1.0) ? 1.0 : (1.0 - ((1.0 - s) * (1.0 - s)));             // Inv x2
                    break;

                case 3:  y = s * s * s;                                                       // x3
                    break;

                default: y = s * s;                                                           // x2
                    break;
            }
            return copysign(fmin(y, SHAPER_HEADROOM), x);
        }
        case eShaperShpExp:
        {
            // notes §32
            static const double kPower[] = {2.0, 3.0, 4.0, 5.0};
            uint32_t            curve    = (settings->curve < 4) ? settings->curve : 0;
            double              y        = ((1.0 - amount) * s) + (amount * pow(s, kPower[curve]));

            return copysign(fmin(y, SHAPER_HEADROOM), x);
        }
        case eShaperSaturate:
        {
            // notes §33
            static const double kPower[] = {3.0, 5.0, 7.0, 9.0};
            uint32_t            curve    = (settings->curve < 4) ? settings->curve : 0;
            double              y        = (s >= 1.0)
                                           ? (1.0 + ((1.0 - amount) * (s - 1.0)))
                                           : (((1.0 - amount) * s) + (amount * (1.0 - pow(1.0 - s, kPower[curve]))));

            return copysign(fmin(y, SHAPER_HEADROOM), x);
        }
        case eShaperWaveWrap:
        {
            // notes §34
            return shaper_fold(x * (1.0 + (amount * 8.0)));
        }
        case eShaperOverdrive:
        {
            // notes §35
            static const double kKnee[]  = {2.0, 16.0, 3.0, 6.0};
            static const double kDrive[] = {8.0, 8.0, 24.0, 32.0};
            uint32_t            type     = (settings->curve < 4) ? settings->curve : 0;
            double              driven   = x * (1.0 + (amount * kDrive[type]));
            double              shaped   = driven / pow(1.0 + pow(fabs(driven), kKnee[type]),
                                                        1.0 / kKnee[type]);

            // Asym shapes only the positive peaks (manual), so the negative half stays linear -
            // and then meets the headroom, which is where its own harmonics come from.
            if ((settings->sym == false) && (driven < 0.0)) {
                shaped = shaper_limit(driven, 1.0);
            }
            return ((1.0 - amount) * x) + (amount * shaped);
        }
        case eShaperClip:
        default:
        {
            // notes §36
            double t = 1.0 - amount;

            if (x > t) {
                return t;
            }

            if ((settings->sym == true) && (x < -t)) {
                return -t;
            }
            return x;
        }
    }
}

// notes §37
// §11.2 - both exact, confirmed 2026-09-18 against the instrument's own coefficient tables. The high
// shelf's first two ARE swapped relative to the names the G2 itself displays: its table holds 8k, 6k,
// 12k while its own text reads "6 kHz", "8 kHz", "12 kHz". That is the instrument's inconsistency, not
// ours - do not "correct" either side to match the other.
static const double kEqLowShelfHz[]  = {80.0, 110.0, 160.0};
static const double kEqHighShelfHz[] = {8000.0, 6000.0, 12000.0};

#define EQ_MID_OCTAVES    (1.0)    // §11.3

// §11.3 - EqPeak's centre, from the instrument's own coefficient table: 20 Hz at 0 to 16 kHz at 127,
// which its table matches to 0.0074% across the whole dial. NOT flt_cutoff_hz(), which the engine and
// the dial both used until 2026-09-18 - that is the filter modules' curve and is up to 45% away here,
// agreeing only near dial 73.
double eq_peak_centre_hz(double dial) {
    return 20.0 * pow(800.0, dial / 127.0);
}

// §8.3 - measured 2026-09-12: 3.3 at Width 127, 4.9 at 112, 9 at 96, and about 190 at 0.
double osc_noise_resonator_q(double widthDial) {
    return 3.34 * exp(0.032 * (127.0 - widthDial));
}

static double eq_dial_gain(double dial) {
    double steps = (dial >= 127.0) ? 64.0 : (dial - 64.0);        // §11.1 - 127 is the full +18

    return pow(10.0, (steps * (18.0 / 64.0)) / 20.0);
}

// §11.3 - EqPeak's BW dial: the damping falls in a straight line, 2 root 2 at the bottom.
static double eq_peak_bw_damping(double bw) {
    return 2.0 * M_SQRT2 * (1.0 - (bw / 128.0));
}

static double eq_peak_damping(double octaves) {
    double ratio = exp2(octaves);

    return 2.0 * (ratio - 1.0) / sqrt(ratio);
}

static double eq_shelf_hz(const double * table, uint32_t selector) {
    return table[(selector > 2u) ? 2u : selector];
}

// §11.4 - a cut mirrors the boost of the same size.
static void eq_mirror_cuts(tEqBands * bands) {
    if ((bands->lowHz > 0.0) && (bands->lowGain < 1.0)) {
        bands->lowHz /= bands->lowGain;
    }

    if ((bands->highHz > 0.0) && (bands->highGain < 1.0)) {
        bands->highHz *= bands->highGain;
    }

    if ((bands->peakHz > 0.0) && (bands->peakGain < 1.0)) {
        bands->peakDamping /= bands->peakGain;
    }
}

bool eq_bands_build(tModule * module, uint32_t variation, tParamReader dial, tEqBands * out) {
    *out = (tEqBands){
        .inputLevel = 1.0, .active = true
    };

    switch (module->type) {
        case moduleTypeEqPeak:
        {
            out->peakHz      = eq_peak_centre_hz(dial(module, variation, 0));
            out->peakGain    = eq_dial_gain(dial(module, variation, 1));
            out->peakDamping = eq_peak_bw_damping(dial(module, variation, 2));
            out->active      = (dial(module, variation, 3) != 0.0);
            out->inputLevel  = mix_level_gain(dial(module, variation, 4));
            break;
        }
        case moduleTypeEq2Band:
        {
            out->lowGain    = eq_dial_gain(dial(module, variation, 0));
            out->highGain   = eq_dial_gain(dial(module, variation, 1));
            out->inputLevel = mix_level_gain(dial(module, variation, 2));
            out->active     = (dial(module, variation, 3) != 0.0);
            out->lowHz      = eq_shelf_hz(kEqLowShelfHz, module->param[variation][4].value);
            out->highHz     = eq_shelf_hz(kEqHighShelfHz, module->param[variation][5].value);
            break;
        }
        case moduleTypeEq3band:
        {
            out->lowGain     = eq_dial_gain(dial(module, variation, 0));
            out->peakGain    = eq_dial_gain(dial(module, variation, 1));
            out->peakHz      = 100.0 * pow(80.0, dial(module, variation, 2) / 127.0);
            out->peakDamping = eq_peak_damping(EQ_MID_OCTAVES);
            out->highGain    = eq_dial_gain(dial(module, variation, 3));
            out->inputLevel  = mix_level_gain(dial(module, variation, 4));
            out->active      = (dial(module, variation, 5) != 0.0);
            out->lowHz       = eq_shelf_hz(kEqLowShelfHz, module->param[variation][6].value);
            out->highHz      = eq_shelf_hz(kEqHighShelfHz, module->param[variation][7].value);
            break;
        }
        default:
            return false;
    }
    eq_mirror_cuts(out);
    return true;
}

static void complex_multiply(double * re, double * im, double otherRe, double otherIm) {
    double real = (*re * otherRe) - (*im * otherIm);

    *im = (*re * otherIm) + (*im * otherRe);
    *re = real;
}

// notes §38
double eq_magnitude(const tEqBands * bands, double hz) {
    double re = 1.0;
    double im = 0.0;

    if (bands->lowHz > 0.0) {
        double r     = hz / bands->lowHz;
        double denom = 1.0 + (r * r);
        double boost = bands->lowGain - 1.0;

        complex_multiply(&re, &im, 1.0 + (boost / denom), -(boost * r / denom));
    }

    if (bands->highHz > 0.0) {
        double r     = hz / bands->highHz;
        double denom = 1.0 + (r * r);
        double boost = bands->highGain - 1.0;

        complex_multiply(&re, &im, 1.0 + (boost * r * r / denom), boost * r / denom);
    }

    if (bands->peakHz > 0.0) {
        double r     = hz / bands->peakHz;
        double q     = bands->peakDamping;
        double real  = 1.0 - (r * r);
        double denom = (real * real) + (q * q * r * r);
        double boost = bands->peakGain - 1.0;

        complex_multiply(&re, &im, 1.0 + (boost * q * q * r * r / denom), boost * q * r * real / denom);
    }
    return sqrt((re * re) + (im * im));
}

// notes §39
#define FLTCOMB_TUNING_SEMITONES    (9.0)    // §13.2 - the comb sits a major sixth below the dial

static const tCombShape kCombShapes[] = {    // §13.4 - Notch, Peak, Deep
    { 1.00, 0.00, 0.0,  0.00},
    {-0.30, 0.90, 1.1,  2.45},
    { 0.60, 0.85, 0.5, -4.10},
};

const tCombShape * flt_comb_shape(uint32_t type) {
    return &kCombShapes[(type < 3u) ? type : 0u];
}

double flt_comb_feedback(double fbParam) {
    return (fbParam - 64.0) / 64.0;    // §13.3
}

double flt_comb_delay_samples(double control, const tCombShape * shape, double sampleRate) {
    double rateScale = sampleRate / FLTCOMB_REFERENCE_RATE;

    return (sampleRate / flt_cutoff_hz(control - FLTCOMB_TUNING_SEMITONES)) - rateScale + (shape->extraDelay * rateScale);
}

// k (1 + b.g.z^-D) / (1 - c.g.z^-D), at a frequency given in cycles per sample.
double flt_comb_magnitude(const tCombShape * shape, double g, double delaySamples, double cyclesPerSample) {
    double theta = 2.0 * M_PI * cyclesPerSample * delaySamples;
    double b     = shape->feedForward * g;
    double c     = shape->feedback * g;
    double numRe = 1.0 + (b * cos(theta));
    double numIm = -b * sin(theta);
    double denRe = 1.0 - (c * cos(theta));
    double denIm = c * sin(theta);
    double k     = pow(10.0, (shape->gainDbPerG2 * g * g) / 20.0);

    return k * sqrt(((numRe * numRe) + (numIm * numIm)) / fmax((denRe * denRe) + (denIm * denIm), 1e-12));
}

// notes §40
#define FLTPHASE_PARAM_FREQ                 (1)
#define FLTPHASE_PARAM_FB                   (3)
#define FLTPHASE_PARAM_NOTCHES              (4)
#define FLTPHASE_PARAM_SPREAD               (5)
#define FLTPHASE_PARAM_TYPE                 (9)
#define FLTPHASE_MAX_SECTIONS               (6)
#define FLTPHASE_CENTRE_SEMITONES           (12.0) // measured at one Freq only
#define FLTPHASE_Q                          (1.04) // measured, at Spread 64
#define FLTPHASE_SPREAD_STEPS_PER_OCTAVE    (32.0) // ASSUMED - Spread is unmeasured

bool flt_phase_settings_build(tModule * module, uint32_t variation, tParamReader dial, tPhaserSettings * out) {
    uint32_t notches = module->param[variation][FLTPHASE_PARAM_NOTCHES].value;
    uint32_t type    = module->param[variation][FLTPHASE_PARAM_TYPE].value;

    if (module->type != moduleTypeFltPhase) {
        return false;
    }
    out->centreHz = flt_cutoff_hz(dial(module, variation, FLTPHASE_PARAM_FREQ) + FLTPHASE_CENTRE_SEMITONES);
    out->q        = FLTPHASE_Q * exp2((64.0 - dial(module, variation, FLTPHASE_PARAM_SPREAD)) / FLTPHASE_SPREAD_STEPS_PER_OCTAVE);
    out->sections = (notches < FLTPHASE_MAX_SECTIONS) ? (notches + 1u) : FLTPHASE_MAX_SECTIONS;
    out->g        = (dial(module, variation, FLTPHASE_PARAM_FB) - 64.0) / 64.0;
    out->type     = (type < 3u) ? type : 0u;
    return true;
}

double flt_phase_magnitude(const tPhaserSettings * settings, double hz) {
    double r     = hz / settings->centreHz;
    double phase = -2.0 * (double)settings->sections * atan2(r / settings->q, 1.0 - (r * r));
    double gRe   = settings->g * cos(phase);    // g times the allpass chain, which has unit magnitude
    double gIm   = settings->g * sin(phase);
    double numRe = 1.0 + gRe;
    double numIm = gIm;
    double denRe = 1.0 - gRe;
    double denIm = -gIm;

    switch (settings->type) {
        case 1:     // Peak
            numRe = 1.0;
            numIm = 0.0;
            break;

        case 2:     // Deep
            break;

        default:    // Notch
            denRe = 1.0;
            denIm = 0.0;
            break;
    }
    return sqrt(((numRe * numRe) + (numIm * numIm)) / fmax((denRe * denRe) + (denIm * denIm), 1e-12));
}

#ifdef __cplusplus
}
#endif

// ─── Operator frequency (DX7) ────────────────────────────────────────────────

// notes §41
double operator_ratio(uint32_t coarse, uint32_t fine) {
    double base = (coarse == 0) ? 0.5 : (double)((coarse > OPERATOR_COARSE_MAX) ? OPERATOR_COARSE_MAX : coarse);

    return base * (1.0 + ((double)((fine > OPERATOR_FINE_MAX) ? OPERATOR_FINE_MAX : fine) / 100.0));
}

double operator_fixed_hz(uint32_t coarse, uint32_t fine) {
    return pow(10.0, (double)(coarse % 4)) * pow(10.0, (double)((fine > OPERATOR_FINE_MAX) ? OPERATOR_FINE_MAX : fine) / 100.0);
}

// ─── Compress ────────────────────────────────────────────────────────────────

// notes §42
double compress_ratio(uint32_t raw) {
    bool     decade = (raw > 34u);
    uint32_t p      = decade ? (raw - 35u) : raw;
    uint32_t tenths = 0;

    if (p <= 9u) {
        tenths = p + 10u;
    } else if (p < 25u) {
        tenths = p * 2u;
    } else {
        tenths = (p * 5u) - 75u;
    }

    if (decade) {
        tenths *= 10u;
    }
    return (double)tenths / 10.0;
}

uint32_t compress_ratio_raw(double ratio) {
    uint32_t best = 0;

    for (uint32_t raw = 1; raw <= COMPRESS_RATIO_RAW_MAX; raw++) {
        if (fabs(log(compress_ratio(raw) / ratio)) < fabs(log(compress_ratio(best) / ratio))) {
            best = raw;
        }
    }

    return best;
}

double compress_out_db(double inDb, uint32_t thresholdRaw, uint32_t refRaw, uint32_t ratioRaw) {
    if (thresholdRaw >= COMPRESS_THRESHOLD_OFF_RAW) {
        return inDb;
    }
    double thresholdDb = (double)thresholdRaw - COMPRESS_DB_OFFSET;
    double targetDb    = fmax((double)refRaw - COMPRESS_DB_OFFSET, thresholdDb);

    return inDb + ((1.0 - (1.0 / compress_ratio(ratioRaw))) * (targetDb - fmax(inDb, thresholdDb)));
}

// The gain-reduction meter shows the reduction the settings call for - dB over Thr x (1 - 1/ratio),
// so nothing at all at 1:1 - as LEDs that each light at their own reduction (sound-engine-notes §122).
// One copy for the engine's meter and, inverted, for the Compress graph's live point.
static const double kCompressMeterStepDb[] = {0.0, 1.0, 2.0, 4.5, 6.0, 9.0, 12.0, 15.0};   // LED 1 .. 8, bottom up
#define COMPRESS_METER_LEDS    (sizeof(kCompressMeterStepDb) / sizeof(kCompressMeterStepDb[0]))

uint32_t compress_meter_lit(double reductionDb) {
    uint32_t lit = 0u;

    if (reductionDb <= 0.0) {
        return 0u;
    }

    for (uint32_t led = 0u; led < (uint32_t)COMPRESS_METER_LEDS; led++) {
        if (reductionDb >= kCompressMeterStepDb[led]) {
            lit = led + 1u;
        }
    }

    return lit;
}

double compress_meter_reduction_db(uint32_t lit) {
    if (lit == 0u) {
        return 0.0;
    }
    return kCompressMeterStepDb[((lit > COMPRESS_METER_LEDS) ? COMPRESS_METER_LEDS : lit) - 1u];
}

// ─── DXRouter ────────────────────────────────────────────────────────────────

// notes §43
static const tDxAlgorithm kDxAlgorithms[DX_ALGORITHMS] = {
    {{0x00, 0x01, 0x00, 0x04, 0x08, 0x10}, 6, 6},   // 1
    {{0x00, 0x01, 0x00, 0x04, 0x08, 0x10}, 2, 2},   // 2
    {{0x00, 0x01, 0x02, 0x00, 0x08, 0x10}, 6, 6},   // 3
    {{0x00, 0x01, 0x02, 0x00, 0x08, 0x10}, 4, 6},   // 4
    {{0x00, 0x01, 0x00, 0x04, 0x00, 0x10}, 6, 6},   // 5
    {{0x00, 0x01, 0x00, 0x04, 0x00, 0x10}, 5, 6},   // 6
    {{0x00, 0x01, 0x00, 0x04, 0x04, 0x10}, 6, 6},   // 7
    {{0x00, 0x01, 0x00, 0x04, 0x04, 0x10}, 4, 4},   // 8
    {{0x00, 0x01, 0x00, 0x04, 0x04, 0x10}, 2, 2},   // 9
    {{0x00, 0x01, 0x02, 0x00, 0x08, 0x08}, 3, 3},   // 10
    {{0x00, 0x01, 0x02, 0x00, 0x08, 0x08}, 6, 6},   // 11
    {{0x00, 0x01, 0x00, 0x04, 0x04, 0x04}, 2, 2},   // 12
    {{0x00, 0x01, 0x00, 0x04, 0x04, 0x04}, 6, 6},   // 13
    {{0x00, 0x01, 0x00, 0x04, 0x08, 0x08}, 6, 6},   // 14
    {{0x00, 0x01, 0x00, 0x04, 0x08, 0x08}, 2, 2},   // 15
    {{0x00, 0x01, 0x01, 0x04, 0x01, 0x10}, 6, 6},   // 16
    {{0x00, 0x01, 0x01, 0x04, 0x01, 0x10}, 2, 2},   // 17
    {{0x00, 0x01, 0x01, 0x01, 0x08, 0x10}, 3, 3},   // 18
    {{0x00, 0x01, 0x02, 0x00, 0x00, 0x18}, 6, 6},   // 19
    {{0x00, 0x00, 0x03, 0x00, 0x08, 0x08}, 3, 3},   // 20
    {{0x00, 0x00, 0x03, 0x00, 0x00, 0x18}, 3, 3},   // 21
    {{0x00, 0x01, 0x00, 0x00, 0x00, 0x1c}, 6, 6},   // 22
    {{0x00, 0x00, 0x02, 0x00, 0x00, 0x18}, 6, 6},   // 23
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x1c}, 6, 6},   // 24
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x18}, 6, 6},   // 25
    {{0x00, 0x00, 0x02, 0x00, 0x08, 0x08}, 6, 6},   // 26
    {{0x00, 0x00, 0x02, 0x00, 0x08, 0x08}, 3, 3},   // 27
    {{0x00, 0x01, 0x00, 0x04, 0x08, 0x00}, 5, 5},   // 28
    {{0x00, 0x00, 0x00, 0x04, 0x00, 0x10}, 6, 6},   // 29
    {{0x00, 0x00, 0x00, 0x04, 0x08, 0x00}, 5, 5},   // 30
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x10}, 6, 6},   // 31
    {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 6, 6},   // 32
};

const tDxAlgorithm * dx_algorithm(uint32_t index) {
    return &kDxAlgorithms[(index < DX_ALGORITHMS) ? index : 0u];
}

// §39 - DrumSynth's Master tune. 0 gives 20 Hz and 127 gives 784 Hz, which is the range the
// manual quotes (p.181).
double drum_master_hz(double paramValue) {
    return 20.0 * pow(2.0, paramValue * 0.041675);
}

// §39 - DrumSynth's Slave, as a multiple of the Master. 127 gives 6.26, the manual's top.
double drum_slave_ratio(double paramValue) {
    return pow(2.0, paramValue / 48.0);
}
