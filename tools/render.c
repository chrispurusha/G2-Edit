/*
 * render — measure G2-Edit's own sound engine with the rig built for the hardware.
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

// WHAT THIS IS FOR.
//
// The reverb's room sizes and decay law were measured by putting a click through the real instrument
// (tools/measure.py) and reading the result (tools/analyse_ir.py). What is left — the delay lengths and
// the topology they sit in — cannot be settled that way alone, because a plausible-sounding wrong
// arrangement is the hardest kind of error to find by ear.
//
// So this puts the SAME click through the engine and writes a file with the SAME shape as a hardware
// capture: four channels, dry on 1-2 and wet on 3-4, plus the .json sidecar measure.py writes. The
// consequence is that ONE analyser command line works on both, and the two answers are directly
// comparable rather than merely similar in spirit:
//
//     ./render --out engine.wav --settings 0,1,2,3 --sweep type
//     python3 analyse_ir.py engine.wav      --dry-channel 1 --wet-channel 3 --raw --skip 0
//     python3 analyse_ir.py g_rooms_t127.wav --dry-channel 5 --wet-channel 7 --raw --skip 2
//
// The dry channels carry the click itself, which is what the analyser locates impulses from — the same
// job the second output pair does on the hardware. Here it is exact rather than a reference, but the
// analysis must not know the difference or it would not be the same analysis.
//
// Rendered at the ENGINE's own rate (96 kHz for a 48 kHz device), so a lag is the same integer as in
// the hardware tables and no rescaling stands between the two sets of numbers.
//
// Build: see tools/do-render, which links the engine's headless dependency set.

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/soundEngine.h"

#define RENDER_DEVICE_RATE    (48000.0)   // engine runs at twice this; see sound_engine_render_reverb_ir
#define RENDER_CHANNELS       (4)

static void put32(FILE * f, uint32_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
    fputc((int)((v >> 16) & 0xFF), f);
    fputc((int)((v >> 24) & 0xFF), f);
}

static void put16(FILE * f, uint16_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}

// 32-bit PCM, four channels, matching tools/capture.c exactly — including the width, because
// analyse_ir.py's fast reader depends on it. A file this tool writes and a file the interface writes
// must be indistinguishable to the analyser or the comparison is not one.
static bool write_wav32(const char * path, const float * samples, size_t frames, uint32_t channels, double rate) {
    FILE *   f         = fopen(path, "wb");

    if (f == NULL) {
        fprintf(stderr, "error: cannot write %s\n", path);
        return false;
    }
    uint32_t dataBytes = (uint32_t)(frames * channels * 4);

    fwrite("RIFF", 1, 4, f);
    put32(f, 36 + dataBytes);
    fwrite("WAVEfmt ", 1, 8, f);
    put32(f, 16);
    put16(f, 1);
    put16(f, (uint16_t)channels);
    put32(f, (uint32_t)rate);
    put32(f, (uint32_t)(rate * channels * 4));
    put16(f, (uint16_t)(channels * 4));
    put16(f, 32);
    fwrite("data", 1, 4, f);
    put32(f, dataBytes);

    for (size_t i = 0; i < (frames * channels); i++) {
        double  v = (double)samples[i];

        if (v > 1.0) {
            v = 1.0;
        }

        if (v < -1.0) {
            v = -1.0;
        }
        put32(f, (uint32_t)(int32_t)(v * 2147483520.0));
    }
    fclose(f);
    return true;
}

// The sidecar analyse_ir.py groups settings by. Written by hand rather than with a JSON library for the
// same reason the analyser is stdlib only: four keys are not worth a dependency.
static bool write_sidecar(const char * wavPath, const char * sweep, const int * values, int count,
                          double period, int repeats, double settle) {
    char   path[1024] = {0};
    size_t len        = strlen(wavPath);

    snprintf(path, sizeof(path), "%s", wavPath);

    if ((len > 4) && (strcmp(path + len - 4, ".wav") == 0)) {
        snprintf(path + len - 4, sizeof(path) - (len - 4), ".json");
    } else {
        snprintf(path + len, sizeof(path) - len, ".json");
    }
    FILE * f = fopen(path, "w");

    if (f == NULL) {
        fprintf(stderr, "error: cannot write %s\n", path);
        return false;
    }
    fprintf(f, "{\n  \"source\": \"engine\",\n  \"sweep\": \"%s\",\n", sweep);
    fprintf(f, "  \"period\": %.4f,\n  \"repeats\": %d,\n  \"settle\": %.4f,\n", period, repeats, settle);
    fprintf(f, "  \"settings\": [\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "    {\"value\": %d}%s\n", values[i], (i + 1 < count) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    printf("wrote %s\n", path);
    return true;
}

// A MINIMAL READER for the files ./tools/capture writes: 32-bit PCM, any channel count. Only one
// channel is wanted (the dry reference), so it is de-interleaved on the way in and converted to the
// float the engine expects. Deliberately not a general wav reader - it accepts exactly what the
// capture tool produces and refuses anything else rather than guessing.
static float * read_wav32_channel(const char * path, uint32_t wantCh, uint32_t * frames, uint32_t * rate) {
    FILE * f = fopen(path, "rb");

    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        return NULL;
    }
    unsigned char hdr[44];

    if (fread(hdr, 1, 44, f) != 44) {
        fclose(f);
        return NULL;
    }
    uint32_t ch   = (uint32_t)hdr[22] | ((uint32_t)hdr[23] << 8);
    uint32_t sr   = (uint32_t)hdr[24] | ((uint32_t)hdr[25] << 8)
                    | ((uint32_t)hdr[26] << 16) | ((uint32_t)hdr[27] << 24);
    uint32_t bits = (uint32_t)hdr[34] | ((uint32_t)hdr[35] << 8);

    if ((bits != 32) || (ch == 0) || (wantCh >= ch)) {
        fprintf(stderr, "%s: %u-bit, %u channels - need 32-bit and channel %u\n", path, bits, ch, wantCh);
        fclose(f);
        return NULL;
    }
    long here = ftell(f);

    fseek(f, 0, SEEK_END);
    long bytes = ftell(f) - here;

    fseek(f, here, SEEK_SET);

    uint32_t n   = (uint32_t)(bytes / (long)(4 * ch));
    float *  out = (float *)malloc((size_t)n * sizeof(float));
    int32_t * row = (int32_t *)malloc((size_t)ch * sizeof(int32_t));

    if ((out == NULL) || (row == NULL)) {
        free(out);
        free(row);
        fclose(f);
        return NULL;
    }

    for (uint32_t i = 0; i < n; i++) {
        if (fread(row, sizeof(int32_t), ch, f) != ch) {
            n = i;
            break;
        }
        out[i] = (float)((double)row[wantCh] / 2147483648.0);
    }
    free(row);
    fclose(f);
    *frames = n;
    *rate   = sr;
    return out;
}

int main(int argc, char ** argv) {
    const char * outPath  = "engine.wav";
    const char * sweep    = "type";
    const char * valueText = "0,1,2,3";
    int          type      = 2;
    int          timeValue = 127;
    int          bright    = 64;
    double       period    = 20.0;      // seconds per impulse; must exceed the decay being measured
    const char * chorusIn  = NULL;      // --chorus <dry.wav>: render the CHORUS from that file instead
    int          inChannel = 0;
    int          detune    = 127;
    int          amount    = 127;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--out") == 0) && ((i + 1) < argc)) {
            outPath = argv[++i];
        } else if ((strcmp(argv[i], "--sweep") == 0) && ((i + 1) < argc)) {
            sweep = argv[++i];
        } else if ((strcmp(argv[i], "--settings") == 0) && ((i + 1) < argc)) {
            valueText = argv[++i];
        } else if ((strcmp(argv[i], "--type") == 0) && ((i + 1) < argc)) {
            type = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--time") == 0) && ((i + 1) < argc)) {
            timeValue = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--bright") == 0) && ((i + 1) < argc)) {
            bright = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--period") == 0) && ((i + 1) < argc)) {
            period = atof(argv[++i]);
        } else if ((strcmp(argv[i], "--chorus") == 0) && ((i + 1) < argc)) {
            chorusIn = argv[++i];
        } else if ((strcmp(argv[i], "--in-channel") == 0) && ((i + 1) < argc)) {
            inChannel = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--detune") == 0) && ((i + 1) < argc)) {
            detune = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--amount") == 0) && ((i + 1) < argc)) {
            amount = atoi(argv[++i]);
        } else {
            fprintf(stderr,
                    "usage: %s [--out f.wav] [--sweep type|time|bright] [--settings 0,1,2,3]\n"
                    "          [--type N] [--time N] [--bright N] [--period S]\n"
                    "\n"
                    "Renders the engine's reverb impulse response into a file shaped like a hardware\n"
                    "capture, so analyse_ir.py compares the two directly. --sweep names which of the\n"
                    "three the --settings list steps; the other two are held at --type/--time/--bright.\n",
                    argv[0]);
            return 2;
        }
    }

    // THE CHORUS TAKES A DIFFERENT PATH ENTIRELY and returns early: it is driven by a real recording
    // rather than an impulse, so none of the sweep machinery below applies to it. Feed it the DRY
    // channel of a hardware capture and the wet it returns can be put straight beside that capture's
    // own wet - same source, same fundamental, same band-limiting, so any difference is the module.
    if (chorusIn != NULL) {
        uint32_t inFrames = 0;
        uint32_t inRate   = 0;
        float *  dry      = read_wav32_channel(chorusIn, (uint32_t)inChannel, &inFrames, &inRate);

        if (dry == NULL) {
            return 1;
        }
        float * wet = (float *)calloc((size_t)inFrames * 2, sizeof(float));

        if (wet == NULL) {
            free(dry);
            return 1;
        }
        // The capture's OWN rate is the device rate here, not RENDER_DEVICE_RATE: the input decides
        // it, and the engine still runs ENGINE_OVERSAMPLE times faster internally. The output is
        // written at the input's rate because that is the grid the samples sit on.
        sound_engine_render_chorus((double)inRate, (uint32_t)detune, (uint32_t)amount,
                                   dry, wet, inFrames);

        bool ok = write_wav32(outPath, wet, inFrames, 2, (double)inRate);

        fprintf(stderr, "chorus: %u frames at %u Hz from %s ch%d, Detune %d Amount %d -> %s\n",
                inFrames, inRate, chorusIn, inChannel, detune, amount, outPath);
        free(dry);
        free(wet);
        return ok ? 0 : 1;
    }

    if ((strcmp(sweep, "type") != 0) && (strcmp(sweep, "time") != 0) && (strcmp(sweep, "bright") != 0)) {
        fprintf(stderr, "error: --sweep must be type, time or bright\n");
        return 2;
    }
    int    values[64] = {0};
    int    count      = 0;

    {
        char   text[256] = {0};
        char * save      = NULL;

        snprintf(text, sizeof(text), "%s", valueText);

        for (char * tok = strtok_r(text, ",", &save); (tok != NULL) && (count < 64); tok = strtok_r(NULL, ",", &save)) {
            values[count++] = atoi(tok);
        }
    }

    if (count == 0) {
        fprintf(stderr, "error: --settings is empty\n");
        return 2;
    }
    double   engineRate  = RENDER_DEVICE_RATE * 2.0;   // ENGINE_OVERSAMPLE, and the note in soundEngine.h
    size_t   perSetting  = (size_t)(period * engineRate);
    size_t   frames      = perSetting * (size_t)count;
    float *  wet         = calloc(perSetting * 2, sizeof(float));
    float *  file        = calloc(frames * RENDER_CHANNELS, sizeof(float));

    if ((wet == NULL) || (file == NULL)) {
        fprintf(stderr, "error: out of memory for %.0f s\n", period * count);
        free(wet);
        free(file);
        return 1;
    }
    printf("engine reverb: sweeping %s over %d settings, %.0f s each at %.0f Hz\n",
           sweep, count, period, engineRate);

    for (int s = 0; s < count; s++) {
        int useType   = (strcmp(sweep, "type") == 0) ? values[s] : type;
        int useTime   = (strcmp(sweep, "time") == 0) ? values[s] : timeValue;
        int useBright = (strcmp(sweep, "bright") == 0) ? values[s] : bright;

        sound_engine_render_reverb_ir(RENDER_DEVICE_RATE, (uint32_t)useType, (uint32_t)useTime,
                                      (uint32_t)useBright, wet, (uint32_t)perSetting);

        double peak = 0.0;

        for (size_t i = 0; i < perSetting; i++) {
            size_t at = ((size_t)s * perSetting) + i;

            // The click on 1-2 and the response on 3-4, laid out exactly as a hardware capture's dry
            // and wet pairs. Full scale on one sample only: the analyser finds impulses here.
            file[(at * RENDER_CHANNELS) + 0] = (i == 0) ? 1.0f : 0.0f;
            file[(at * RENDER_CHANNELS) + 1] = (i == 0) ? 1.0f : 0.0f;
            file[(at * RENDER_CHANNELS) + 2] = wet[(i * 2) + 0];
            file[(at * RENDER_CHANNELS) + 3] = wet[(i * 2) + 1];

            if (wet[i * 2] > peak) {
                peak = wet[i * 2];
            }
        }
        printf("  %s = %-3d  (type %d, time %d, bright %d)  wet peak %.4f\n",
               sweep, values[s], useType, useTime, useBright, peak);
    }

    if (!write_wav32(outPath, file, frames, RENDER_CHANNELS, engineRate)) {
        free(wet);
        free(file);
        return 1;
    }
    printf("wrote %s: %zu frames (%.1f s), %d channels, %.0f Hz\n",
           outPath, frames, (double)frames / engineRate, RENDER_CHANNELS, engineRate);

    // settle 0 and one impulse per setting: the sidecar's grouping maths is the same either way, and
    // being explicit here means analyse_ir.py needs no special case for an engine render.
    write_sidecar(outPath, sweep, values, count, period, 1, 0.0);
    free(wet);
    free(file);
    return 0;
}
