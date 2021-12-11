//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef multi_window_plugin
#define multi_window_plugin

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "amplify.h"

extern const int IS_LEVELER;
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
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;
    unsigned long rate;
} Leveler;

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Leveler * h = malloc(sizeof(Leveler));
    h->channels[0] = &h->left;
    h->channels[1] = &h->right;
    h->rate = rate;

    int i = 0;
    for (i = 0; i < maxChannels; i++) {
        struct Channel* channel = h->channels[i];
        struct Window* window1;
        window1 = &channel->window1;
        initWindow(window1, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE);

        struct Window* window2;
        window2 = &channel->window2;
        initWindow(window2, BUFFER_DURATION2, h->rate, MAX_CHANGE, ADJUST_RATE);

        struct Window* window3;
        window3 = &channel->window3;
        initWindow(window3, BUFFER_DURATION3, h->rate, MAX_CHANGE, ADJUST_RATE);

        channel->amplification = 0.0;
        channel->oldAmplification = 0.0;
        channel->oldAmplificationSmoothed = 0.0;
    }
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;

    if (h->left.window1.active){
        free(h->left.window1.data);
        free(h->left.window1.square);
        free(h->right.window1.data);
        free(h->right.window1.square);
    }
    if (h->left.window2.active){
        free(h->left.window2.data);
        free(h->left.window2.square);
        free(h->right.window2.data);
        free(h->right.window2.square);
    }
    if (h->left.window3.active){
        free(h->left.window3.data);
        free(h->left.window3.square);
        free(h->right.window3.data);
        free(h->right.window3.square);
    }
     free(handle);
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

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1 = &channel->window1;
        struct Window* window2 = &channel->window2;
        struct Window* window3 = &channel->window3;

        unsigned long s;
        for (s = 0; s < samples; s++) {

            if (window1->active){
                prepareWindow(window1);
                addWindowData(window1, channel->in[s]);
                sumWindowData(window1);
            }
            if (window2->active){
                prepareWindow(window1);
                addWindowData(window1, channel->in[s]);
                sumWindowData(window1);
            }
            if (window3->active){
                prepareWindow(window1);
                addWindowData(window1, channel->in[s]);
                sumWindowData(window1);
            }

            // interpolate with shifted adjust position
            double ampFactor = interpolateAmplification(channel->amplification, channel->oldAmplification,
                    window1->adjustPosition, window1->adjustRate);

            // read from playPosition, amplify and limit
            double value = (window1->data[window1->playPosition] - getWindowDcOffset(window1)) * ampFactor;
            value = limit(value);
            channel->out[s] = (LADSPA_Data) value;

#ifdef DEBUG
            printWindow(window1, (channel == h->channels[1]));
#endif

            if (window1->active && window1->adjustPosition == 0){
                calcWindowAmplification(window1, getRmsValue(window1->sumSquare, window1->size), IS_LEVELER);
            }
            if (window2->active && window2->adjustPosition == 0){
                calcWindowAmplification(window2, getRmsValue(window2->sumSquare, window2->size), IS_LEVELER);
            }
            if (window3->active && window3->adjustPosition == 0){
                calcWindowAmplification(window3, getRmsValue(window3->sumSquare, window3->size), IS_LEVELER);
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
