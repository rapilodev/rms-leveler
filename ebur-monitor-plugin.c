#ifndef ebur_plugin
#define ebur_plugin

//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <ladspa.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "monitor-plugin.h"
#include "ebur128.h"
#include "file-log.h"
#include "socket.h"

#define SECONDS 1000.0;
extern const char *LOG_ID;

struct EburChannel {
    LADSPA_Data *in;
    LADSPA_Data *out;
    ebur128_state *ebur128;
};

typedef struct {
    struct EburChannel left;
    struct EburChannel right;
    unsigned long rate;
    double t;
    FileLogger file_logger;
    double buffer_duration;
} EburMonitor;

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long rate) {
    EburMonitor *h = calloc(1, sizeof(EburMonitor));
    if (h == NULL) return NULL;
    h->rate = rate;
    h->buffer_duration = WINDOW_INIT;

    char *dir = getenv("MONITOR_LOG_DIR");
    if (dir == NULL) dir = "/var/log/monitor";
    file_logger_init(&h->file_logger, dir, LOG_ID);

    h->left.ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_I);
    h->right.ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_I);
    setup_socket();
    return (LADSPA_Handle) h;
}

static void cleanup(LADSPA_Handle handle) {
    EburMonitor *h = (EburMonitor*) handle;
    if (h == NULL) return;
    ebur128_destroy(&h->left.ebur128);
    ebur128_destroy(&h->right.ebur128);
    file_logger_cleanup(&h->file_logger);
    free(handle);
    close_socket();
}

static void connect_port(const LADSPA_Handle handle, unsigned long num,
        LADSPA_Data *port) {
    EburMonitor *h = (EburMonitor*) handle;
    if (num == 0) h->left.in = port;
    if (num == 1) h->right.in = port;
    if (num == 2) h->left.out = port;
    if (num == 3) h->right.out = port;
    if (num == 4) h->buffer_duration = *port;
    if (h->buffer_duration < WINDOW_MIN) h->buffer_duration = WINDOW_MIN;
    if (h->buffer_duration > WINDOW_MAX) h->buffer_duration = WINDOW_MAX;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    EburMonitor *h = (EburMonitor*) handle;
    if (h == NULL || samples == 0) return;

    struct EburChannel* channels[] = {&h->left, &h->right};
    for (int c = 0; c < 2; c++) {
        struct EburChannel *channel = channels[c];
        if (channel->in == NULL || channel->out == NULL) continue;

        for (uint32_t s = 0; s < samples; s++) {
            LADSPA_Data input = channel->in[s];
            channel->out[s] = input; // Pass-through
            ebur128_add_frames_float(channel->ebur128, &input, (size_t) 1);
        }
    }

    h->t += samples;
    double limit = h->rate * MIN(h->buffer_duration, LOG_INTERVAL);
    if (h->t >= limit) {
        while(h->t >= limit) h->t -= limit;
        double loudness_l = 0.;
        double loudness_r = 0.;

        for (int c = 0; c < ARRAY_LENGTH(channels); c++) {
            struct EburChannel *channel = channels[c];
            double loudness = 0;
            ebur128_loudness_global(channel->ebur128, &loudness);

            if (c == 0) loudness_l = loudness;
            if (c == 1) loudness_r = loudness;

            ebur128_destroy(&channel->ebur128);
            channel->ebur128 = ebur128_init(1, h->rate, EBUR128_MODE_I);
        }

        print_log(LOG_ID, loudness_l, loudness_r);
        file_log(&h->file_logger, loudness_l, loudness_r);
        send_broadcast_message(LOG_ID, loudness_l, loudness_r);
    }
}

#endif
