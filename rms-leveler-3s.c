//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <stdlib.h> 
#include <ladspa.h>
#include <stdio.h>
#include <math.h>

#include "leveler.h"
#include "amplify.h"
#include "single-window-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 1;
// long term measurement window
const double BUFFER_DURATION1 = 3.0;

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_leveler_3s", .Name =
        "RMS leveler -20dBFS, stereo, 3 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}

