#ifndef single_window_plugin
#define single_window_plugin

#include <stdlib.h>
#include <ladspa.h>
#include <stdio.h>
#include <math.h>
#include "leveler.h"
#include "amplify.h"

extern const int IS_LEVELER;
extern const double BUFFER_DURATION1;

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
    if (num == IN_LEFT)   h->left.in = port;
    if (num == IN_RIGHT)  h->right.in = port;
    if (num == OUT_LEFT)  h->left.out = port;
    if (num == OUT_RIGHT) h->right.out = port;
}

static void run(LADSPA_Handle handle, unsigned long samples) {
    Leveler * h = (Leveler *) handle;

    int c = 0;
    for (c = 0; c < maxChannels; c++) {
        struct Channel* channel = h->channels[c];
        struct Window* window1 = &channel->window1;

        unsigned long s;
        for (s = 0; s < samples; s++) {

            prepareWindow(window1);
            addWindowData(window1, channel->in[s]);
            sumWindowData(window1);

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

            if (window1->adjustPosition == 0)
                calcWindowAmplification(window1, IS_LEVELER);
            channel->amplification    = window1->amplification;
            channel->oldAmplification = window1->oldAmplification;
            moveWindow(window1);
        }
    }
}

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel", "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
        LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

#endif
