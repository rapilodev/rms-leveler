#ifndef ebur_plugin
#define ebur_plugin

//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <stdint.h>
#include <stdlib.h>
#include <ladspa.h>
#include <math.h>
#include "ebur128.h"
#include "amplify.h"
#include "stereo-plugin.h"

const double SECONDS = 1000.0;
extern const double BUFFER_DURATION1;
extern const char *LOG_FILENAME;

struct EburChannel {
    LADSPA_Data *in;
    LADSPA_Data *out;
    ebur128_state *ebur128;
};

// define our handler type
typedef struct {
    struct EburChannel *channels[2];
    struct EburChannel left;
    struct EburChannel right;
    unsigned long rate;
    double t;
    char *log_dir;
} EburLeveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate) {
    EburLeveler *h = malloc(sizeof(EburLeveler));
    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->left.out = NULL;
    h->right.out = NULL;
    h->left.in = NULL;
    h->right.in = NULL;
    h->rate = rate;
    h->t = 0.;
    h->log_dir = getenv("MONITOR_LOG_DIR");
    if (h->log_dir == NULL)
        h->log_dir = "/var/log/monitor";

    fprintf(stderr, "%f %f\n", maxChannels, (BUFFER_DURATION1));
    for (int i = 0; i < maxChannels; i++) {
        struct EburChannel *channel = h->channels[i];
        channel->ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_I);
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    EburLeveler *h = (EburLeveler*) handle;
    for (int i = 0; i < maxChannels; i++) {
        ebur128_destroy(&h->channels[i]->ebur128);
    }
    free(handle);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num,
        LADSPA_Data *port) {
    EburLeveler *h = (EburLeveler*) handle;
    if (num == 0)
        h->left.in = port;
    if (num == 1)
        h->right.in = port;
    if (num == 2)
        h->left.out = port;
    if (num == 3)
        h->right.out = port;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    EburLeveler *h = (EburLeveler*) handle;

    for (int c = 0; c < maxChannels; c++) {
        struct EburChannel *channel = h->channels[c];
        for (uint32_t s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s];
            if (channel->out != NULL)
                channel->out[s] = input;
            ebur128_add_frames_float(channel->ebur128, &input, (size_t) 1);
        }
    }

    h->t += samples;
    double limit = BUFFER_DURATION1 * h->rate;
    if (h->t > limit) {
        h->t -= limit;
        double loudness_l = 0.;
        double loudness_r = 0.;
        for (int c = 0; c < maxChannels; c++) {
            struct EburChannel *channel = h->channels[c];
            double loudness = 0;
            ebur128_loudness_global(channel->ebur128, &loudness);
            if (c == 0)
                loudness_l = loudness;
            if (c == 1)
                loudness_r = loudness;
            ebur128_destroy(&channel->ebur128);
            channel->ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_I);
        }
        print_log(loudness_l, loudness_r);
        file_log(h->log_dir, LOG_FILENAME, loudness_l, loudness_r);
    }
}

#endif
