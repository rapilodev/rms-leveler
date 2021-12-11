#ifndef ebur_plugin
#define ebur_plugin

//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "ebur128.h"
#include "amplify.h"

extern const int IS_LEVELER;
extern const double BUFFER_DURATION1;
const double SECONDS = 1000.0;

struct EburChannel {
    LADSPA_Data* in;
    LADSPA_Data* out;
    ebur128_state* ebur128;

    double amplification;
    double oldAmplification;

    struct Window window;
};

// define our handler type
typedef struct {
    struct EburChannel* channels[2];
    struct EburChannel left;
    struct EburChannel right;
    unsigned long rate;
} EburLeveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    EburLeveler * h = malloc(sizeof(EburLeveler));

    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->rate = rate;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct EburChannel* channel = h->channels[i];

        struct Window* window;
        window = &channel->window;
        initWindow(window, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE);

        channel->ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_LRA);
        ebur128_set_max_window(channel->ebur128, (unsigned long) (window->duration*SECONDS));

        channel->amplification = 1.0;
        channel->oldAmplification = 1.0;
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    EburLeveler * h = (EburLeveler *) handle;
    free(h->left.window.data);
    free(h->right.window.data);

    ebur128_destroy(&h->left.ebur128);
    ebur128_destroy(&h->right.ebur128);
    free(handle);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    EburLeveler * h = (EburLeveler *) handle;
    if (num == 0)   h->left.in = port;
    if (num == 1)   h->right.in = port;
    if (num == 2)   h->left.out = port;
    if (num == 3)   h->right.out = port;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    EburLeveler * h = (EburLeveler *) handle;
    double loudness_window;

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct EburChannel* channel = h->channels[c];
        struct Window* window = &channel->window;

        unsigned long s;
        for (s = 0; s < samples; s++) {

            prepareWindow(window);
            addWindowData(window, channel->in[s]);

            ebur128_add_frames_float(channel->ebur128, &channel->in[s], (size_t) 1);

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification, window->adjustPosition,
                    window->adjustRate);

            // read from playPosition, amplify and limit
            double value = (window->data[window->playPosition] - getWindowDcOffset(window)) * ampFactor;
            value = limit(value);
            channel->out[s] = (LADSPA_Data) value;

            // calculate amplification from EBU R128 values
            if (window->adjustPosition == 0) {
                ebur128_loudness_window(channel->ebur128, (unsigned long) window->duration*SECONDS, &loudness_window);
                calcWindowAmplification(window, loudness_window, IS_LEVELER);

                channel->amplification    = window->amplification;
                channel->oldAmplification = window->oldAmplification;

#ifdef DEBUG
                if (c==0) fprintf(
					stderr, "%.1f\t%2.3f\t%2.3f\t%2.3f\n",
					window->position, loudness_window, channel->in[s], value
				);
#endif
            }
            moveWindow(window);
        }
    }
}

#endif
