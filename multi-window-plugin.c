//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef multi_window_plugin
#define multi_window_plugin

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "amplify.h"
#include "stereo-plugin.h"

extern const int IS_LEVELER;
extern const int LOOK_AHEAD;
extern const double BUFFER_DURATION1;
extern const double BUFFER_DURATION2;
extern const double BUFFER_DURATION3;

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
} Leveler;

void destroyLeveler(Leveler *h) {
    if (h == NULL) return;
    freeWindow(&h->left.window1);
    freeWindow(&h->left.window2);
    freeWindow(&h->left.window3);
    freeWindow(&h->right.window1);
    freeWindow(&h->right.window2);
    freeWindow(&h->right.window3);
    free(h);
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler *h = calloc(1, sizeof(Leveler));
    if (h == NULL) return NULL;
    h->rate = rate;
    h->input_gain = 1.0;

    struct Channel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel* channel = channels[c];
        channel->amplification = 0.0;
        channel->oldAmplification = 0.0;
        channel->oldAmplificationSmoothed = 0.0;
        if(!initWindow(&channel->window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE)) {
            destroyLeveler(h);
            return NULL;
        }
        if(!initWindow(&channel->window2, LOOK_AHEAD, BUFFER_DURATION2, h->rate, MAX_CHANGE, ADJUST_RATE)) {
            destroyLeveler(h);
            return NULL;
        }
        if(!initWindow(&channel->window3, LOOK_AHEAD, BUFFER_DURATION3, h->rate, MAX_CHANGE, ADJUST_RATE)) {
            destroyLeveler(h);
            return NULL;
        }
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    destroyLeveler(h);
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    Leveler * h = (Leveler *) handle;
    if (num == 0) h->left.in = port;
    if (num == 1) h->right.in = port;
    if (num == 2) h->left.out = port;
    if (num == 3) h->right.out = port;
}

void getAvgAmp(struct Channel* channel, struct Window* window1, struct Window* window2, struct Window* window3) {
    double sum = 0;
    double amp = 0;
    double oldAmp = 0;
    if (window1->active){
        sum++;
        amp += window1->amplification;
        oldAmp += window1->oldAmplification;
    }
    if (window2->active){
        sum++;
        amp += window2->amplification;
        oldAmp += window2->oldAmplification;
    }
    if (window3->active){
        sum++;
        amp += window2->amplification;
        oldAmp += window3->oldAmplification;
    }

    channel->amplification = amp / sum;
    channel->oldAmplification = oldAmp / sum;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;
    if (h == NULL || h->input_gain_port == NULL || samples == 0) return;

    struct Channel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel* channel = channels[c];
        if (channel->in == NULL || channel->out == NULL) continue;
        struct Window* window1 = &channel->window1;
        struct Window* window2 = &channel->window2;
        struct Window* window3 = &channel->window3;

        for (unsigned long s = 0; s < samples; s++) {

            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s] * h->input_gain;
            if (window1->active){
                prepareWindow(window1);
                addWindowData(window1, input);
                sumWindowData(window1);
            }
            if (window2->active){
                prepareWindow(window2);
                addWindowData(window2, input);
                sumWindowData(window2);
            }
            if (window3->active){
                prepareWindow(window3);
                addWindowData(window3, input);
                sumWindowData(window3);
            }

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
            printWindow(window1, c ==  ARRAY_LENGTH(channels)-1 );
#endif

            if (window1->active && window1->adjustPosition == 0){
                calcWindowAmplification(window1, getRmsValue(window1->sumSquare, window1->size), IS_LEVELER, h->input_gain);
            }
            if (window2->active && window2->adjustPosition == 0){
                calcWindowAmplification(window2, getRmsValue(window2->sumSquare, window2->size), IS_LEVELER, h->input_gain);
            }
            if (window3->active && window3->adjustPosition == 0){
                calcWindowAmplification(window3, getRmsValue(window3->sumSquare, window3->size), IS_LEVELER, h->input_gain);
            }

            if ( (window1->active && window1->adjustPosition == 0)
              || (window2->active && window2->adjustPosition == 0)
              || (window3->active && window3->adjustPosition == 0)
            ) getAvgAmp(channel, window1, window2, window3);

            if (window1->active) moveWindow(window1);
            if (window2->active) moveWindow(window2);
            if (window3->active) moveWindow(window3);
        }
    }
}

#endif
