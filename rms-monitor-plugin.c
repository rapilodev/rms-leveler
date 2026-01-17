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

extern const int LOOK_AHEAD;
extern const double BUFFER_DURATION1;
extern const char *LOG_ID;

struct Channel {
    LADSPA_Data* in;
    LADSPA_Data* out;
    struct Window window1;
};

// define our handler type
typedef struct {
    struct Channel* channels[2];
    struct Channel left;
    struct Channel right;
    unsigned long rate;
    double t;
    char *log_dir;
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
    h->t = 0.;
    h->log_dir = getenv("MONITOR_LOG_DIR");
    if (h->log_dir == NULL)
        h->log_dir = "/var/log/monitor";

    for (int c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1;
        window1 = &channel->window1;
        initWindow(window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE);
    }
    setup_socket();
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    free(h->left.window1.data);
    free(h->left.window1.square);
    free(h->right.window1.data);
    free(h->right.window1.square);
    free(handle);
    close_socket();
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    Leveler * h = (Leveler *) handle;
    if (num == 0)   h->left.in = port;
    if (num == 1)  h->right.in = port;
    if (num == 2)  h->left.out = port;
    if (num == 3) h->right.out = port;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;

    for (int c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1 = &channel->window1;

        for (unsigned long s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s];
            prepareWindow(window1);
            addWindowData(window1, input);
            sumWindowData(window1);
            if (channel->out != NULL) channel->out[s] = (LADSPA_Data) input;
            moveWindow(window1);
        }
    }

    h->t += samples;
    double limit = BUFFER_DURATION1 * h->rate;
    if (h->t > limit) {
        h->t -= limit;

        double rms_left = 0.;
        double rms_right = 0.;
        for (int c = 0; c < maxChannels; c++) {
            struct Channel* channel = h->channels[c];
            struct Window* window1 = &channel->window1;
            double rms = getRmsValue(window1->sumSquare, window1->size);
            if (c==0) rms_left = rms;
            if (c==1) rms_right = rms;
        }
        print_log(LOG_ID, rms_left, rms_right);
        file_log(h->log_dir, LOG_ID, rms_left, rms_right);
        send_broadcast_message(LOG_ID, rms_left, rms_right);
    }
}

#endif
