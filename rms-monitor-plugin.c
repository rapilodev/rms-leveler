//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef single_window_plugin
#define single_window_plugin

#include <ladspa.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "window.h"
#include "file-log.h"
#include "socket.h"
#include "monitor-plugin.h"

extern const int LOOK_AHEAD;
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
    FileLogger file_logger;
    double buffer_duration;
} Monitor;

void destroyMonitor(Monitor *h) {
    if (h == NULL) return;
    freeWindow(&h->left.window1);
    freeWindow(&h->right.window1);
    free(h);
}

// update duration and virtually downsize window
void set_buffer_duration(Monitor* h, double duration) {
    if (duration < WINDOW_MIN) duration = WINDOW_MIN;
    if (duration > WINDOW_MAX) duration = WINDOW_MAX;
    h->buffer_duration = duration;
    h->left.window1.dataSize = duration * h->rate;
    h->right.window1.dataSize = duration * h->rate;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor * d, unsigned long rate) {
    Monitor* h = calloc(1, sizeof(Monitor));
    if (h == NULL) return NULL;
    h->rate = rate;
    h->t = 0.;
    char *dir = getenv("MONITOR_LOG_DIR");
    if (dir == NULL) dir = "/var/log/monitor";
    file_logger_init(&h->file_logger, dir, LOG_ID);

    if (!initWindow(&h->left.window1, LOOK_AHEAD, WINDOW_MAX, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyMonitor(h);
        return NULL;
    }
    if (!initWindow(&h->right.window1, LOOK_AHEAD, WINDOW_MAX, h->rate, MAX_CHANGE, ADJUST_RATE)) {
        destroyMonitor(h);
        return NULL;
    }
    set_buffer_duration(h, WINDOW_INIT);
    setup_socket();
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    Monitor * h = (Monitor *) handle;
    file_logger_cleanup(&h->file_logger);
    destroyMonitor(h);
    close_socket();
}

static void connect_port(const LADSPA_Handle handle, unsigned long num, LADSPA_Data * port) {
    Monitor * h = (Monitor *) handle;
    if (num == 0) h->left.in = port;
    if (num == 1) h->right.in = port;
    if (num == 2) h->left.out = port;
    if (num == 3) h->right.out = port;
    if (num == 4) set_buffer_duration(h, *port);
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Monitor * h = (Monitor *) handle;
    if (h == NULL || samples == 0) return;

    struct Channel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
        struct Channel* channel = channels[c];
        if (channel->in == NULL || channel->out == NULL) continue;
        struct Window* window1 = &channel->window1;

        for (unsigned long s = 0; s < samples; s++) {
            LADSPA_Data input = (channel == NULL) ? 0 : channel->in[s];
            if (channel->out != NULL) channel->out[s] = (LADSPA_Data) input;
            prepareWindow(window1);
            addWindowData(window1, input);
            sumWindowData(window1);
            moveWindow(window1);
        }
    }

    h->t += samples;
    double limit = h->rate * MIN(h->buffer_duration, LOG_INTERVAL);
    if (h->t >= limit) {
        while(h->t >= limit) h->t -= limit;
        double rms_left  = getRmsValue(h->left.window1.sumSquare,  h->left.window1.size);
        double rms_right = getRmsValue(h->right.window1.sumSquare, h->right.window1.size);
        print_log(LOG_ID, rms_left, rms_right);
        file_log(&h->file_logger, rms_left, rms_right);
        send_broadcast_message(LOG_ID, rms_left, rms_right);
    }
}

#endif
