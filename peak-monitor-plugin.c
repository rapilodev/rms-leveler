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

extern const double BUFFER_DURATION1;
extern const char *LOG_ID;

struct Channel {
    LADSPA_Data *in;
    LADSPA_Data *out;
};

// define our handler type
typedef struct {
    struct Channel left;
    struct Channel right;
    double peak_left;
    double peak_right;
    unsigned long rate;
    double t;
    char *log_dir;
} Leveler;


static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate) {
    Leveler *h = calloc(1, sizeof(Leveler));
    if (h == NULL) return NULL;
    h->rate = rate;
    h->log_dir = getenv("MONITOR_LOG_DIR");
    if (h->log_dir == NULL)
        h->log_dir = "/var/log/monitor";
    setup_socket();
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    free(handle);
    close_socket();
}

static void connect_port(const LADSPA_Handle handle, unsigned long num,
        LADSPA_Data *port) {
    Leveler *h = (Leveler*) handle;
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
    Leveler *h = (Leveler*) handle;
    if (h == NULL || h->input_gain_port == NULL || samples == 0) return;
    double peaks[] = { h->peak_left, h->peak_right };
    struct Channel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel *channel = channels[c];
        if (channel->in == NULL || channel->out == NULL) continue;
        if (channel == NULL || c >= 2)
            continue;
        for (unsigned long s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s];
            if (channel->out != NULL)
                channel->out[s] = (LADSPA_Data) input;
            if (fabs(input) > peaks[c])
                peaks[c] = fabs(input);
        }
    }
    h->peak_left = peaks[0];
    h->peak_right = peaks[1];

    h->t += samples;
    double limit = BUFFER_DURATION1 * h->rate;
    if (h->t > limit) {
        h->t -= limit;
        double l = getDb(h->peak_left);
        double r = getDb(h->peak_right);
        print_log(LOG_ID, l, r);
        file_log(h->log_dir, LOG_ID, l, r);
        send_broadcast_message(LOG_ID, l, r);
        h->peak_left = 0.0;
        h->peak_right = 0.0;
    }
}

#endif
