//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "single-window-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 0;
// long term measurement window
const double BUFFER_DURATION1 = 1.0;

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b3e5, .Label = "rms_limiter_1s", .Name =
        "RMS limiter -20dBFS, stereo, 1 seconds window", .Maker = "Milan Chrobok", .Copyright = "GPL 2 or 3", .PortCount = 4,
        .PortDescriptors = c_port_descriptors, .PortNames = c_port_names, .instantiate = instantiate, .connect_port = connect_port, .run =
                run, .cleanup = cleanup };

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}

