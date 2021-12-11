#include <stdlib.h> 
#include <ladspa.h>
#include <stdio.h>
#include <math.h>

//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "leveler.h"
#include "amplify.h"
#include "multi-window-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 0;
// long term measurement window
const double BUFFER_DURATION1 = 6.0;
const double BUFFER_DURATION2 = 3.0;
const double BUFFER_DURATION3 = 0.4;

static const char * c_port_names[4] = { "Input - Left Channel", "Input - Right Channel", "Output - Left Channel", "Output - Right Channel" };

static LADSPA_PortDescriptor c_port_descriptors[4] = { LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
        LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT, LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT };

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_limiter_6s", .Name =
        "RMS limiter -20dBFS, stereo, 6 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}
