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
    struct Channel left;
    struct Channel right;
    unsigned long rate;
    double t;
    char *log_dir;
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
    h->t = 0.;
    h->log_dir = getenv("MONITOR_LOG_DIR");
    if (h->log_dir == NULL)
        h->log_dir = "/var/log/monitor";

    if (!initWindow(&h->left.window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyLeveler(h);
        return NULL;
    }
    if (!initWindow(&h->right.window1, LOOK_AHEAD, BUFFER_DURATION1, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyLeveler(h);
        return NULL;
    }

    setup_socket();
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Leveler * h = (Leveler *) handle;
    destroyLeveler(h);
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
    if (h == NULL || h->input_gain_port == NULL || samples == 0) return;

    struct Channel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel* channel = channels[c];
        if (channel->in == NULL || channel->out == NULL) continue;
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
        double rms_left  = getRmsValue(h->left.window1.sumSquare,  h->left.window1.size);
        double rms_right = getRmsValue(h->right.window1.sumSquare, h->right.window1.size);
        print_log(LOG_ID, rms_left, rms_right);
        file_log(h->log_dir, LOG_ID, rms_left, rms_right);
        send_broadcast_message(LOG_ID, rms_left, rms_right);
    }
}

#endif
