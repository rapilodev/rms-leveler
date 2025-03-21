//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef single_window_plugin
#define single_window_plugi

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "amplify.h"
#include "stereo-plugin.h"

extern const int IS_LEVELER;
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
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;
    unsigned long rate;
    double input_gain;
} Leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = malloc(sizeof(Leveler));
    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->left.out = NULL;
    h->right.out = NULL;
    h->left.in = NULL;
    h->right.in = NULL;
    h->rate = rate;
    h->input_gain = 1.0;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct Channel* channel = h->channels[i];
        struct Window* window1;
        window1 = &channel->window1;
        initWindow(window1, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE);

        channel->amplification = 0.0;
        channel->oldAmplification = 0.0;
        channel->oldAmplificationSmoothed = 0.0;
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    free(h->left.window1.data);
    free(h->left.window1.square);
    free(h->right.window1.data);
    free(h->right.window1.square);
    free(handle);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    Leveler * h = (Leveler *) handle;
    if (num == 0) h->left.in = port;
    if (num == 1) h->right.in = port;
    if (num == 2) h->left.out = port;
    if (num == 3) h->right.out = port;
    if (num == 4) h->input_gain = pow(10.0, *port / 20.0);
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1 = &channel->window1;

        unsigned long s;
        for (s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s] * h->input_gain;
            prepareWindow(window1);
            addWindowData(window1, input);
            sumWindowData(window1);

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification,
                window1->adjustPosition, window1->adjustRate);

            // read from playPosition, amplify and limit
            double value = (window1->data[window1->playPosition] - getWindowDcOffset(window1)) * ampFactor;
            value = limit(value);
            if (channel->out != NULL) {
                channel->out[s] = (LADSPA_Data) value;
            }
#ifdef DEBUG
            printWindow(window1, (channel == h->channels[1]));
#endif

            if (window1->adjustPosition == 0)
                calcWindowAmplification(window1, getRmsValue(window1->sumSquare, window1->size), IS_LEVELER);
            channel->amplification    = window1->amplification;
            channel->oldAmplification = window1->oldAmplification;
            moveWindow(window1);
        }
    }
}

#endif
