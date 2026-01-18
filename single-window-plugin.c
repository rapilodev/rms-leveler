//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef single_window_plugin
#define single_window_plugin

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "amplify.h"
#include "stereo-plugin.h"

extern const int IS_LEVELER;
extern const int LOOK_AHEAD;
extern const double BUFFER_DURATION1;

struct Channel {
    LADSPA_Data* in;
    LADSPA_Data* out;

    double amplification;
    double oldAmplification;
    double oldAmplificationSmoothed;

    struct Window window1;
    struct Window window2;
    struct Window window3;
};

// define our handler type
typedef struct {
    struct Channel left;
    struct Channel right;
    unsigned long rate;
    double input_gain;
    LADSPA_Data* input_gain_port;
} Leveler;

void destroyLeveler(Leveler *h) {
    if (h == NULL) return;
    freeWindow(&h->left.window1);
    freeWindow(&h->right.window1);
    free(h);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = calloc(1, sizeof(Leveler));
    if (h == NULL) return NULL;
    h->rate = rate;
    h->input_gain = 1.0;

    if (!initWindow(&h->left.window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyLeveler(h);
        return NULL;
    }
    if (!initWindow(&h->right.window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyLeveler(h);
        return NULL;
    }

    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    destroyLeveler(h);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data *port) {
    Leveler * h = (Leveler *) handle;
    if (num == 0) h->left.in = port;
    if (num == 1) h->right.in = port;
    if (num == 2) h->left.out = port;
    if (num == 3) h->right.out = port;
    if (num == 4) h->input_gain_port = port;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;
    struct Channel* channels[] = {&h->left, &h->right};
    h->input_gain = pow(10.0, *(h->input_gain_port) / 20.0);
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel* channel = channels[c];
        struct Window* window1 = &channel->window1;

        for (unsigned long s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s] * h->input_gain;
            prepareWindow(window1);
            addWindowData(window1, input);
            sumWindowData(window1);
            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification,
                window1->adjustPosition, window1->adjustRate);
            // read from playPosition, amplify and limit
            double value =
                (LOOK_AHEAD == 1)
                ? window1->data[window1->playPosition] - getWindowDcOffset(window1)
                : input;
            value = limit(ampFactor * value);
            if (channel->out != NULL) {
                channel->out[s] = (LADSPA_Data) value;
            }
#ifdef DEBUG
            printWindow(window1, c==ARRAY_LENGTH(channels)-1);
#endif

            if (window1->adjustPosition == 0)
                calcWindowAmplification(window1, getRmsValue(window1->sumSquare, window1->size), IS_LEVELER, h->input_gain);
            channel->amplification    = window1->amplification;
            channel->oldAmplification = window1->oldAmplification;
            moveWindow(window1);
        }
    }
}

#endif
