//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "single-window-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 1;
// long term measurement window
const double BUFFER_DURATION1 = 0.3;

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b309,
    .Label = "rms_leveler_0.3s", .Name = "RMS leveler, -20dBFS, 0.3 seconds window",
    .Maker = "Milan Chrobok", .Copyright = "GPL 3",
    .PortCount = 5, .connect_port = connect_port,
    .PortNames = c_port_names, .PortRangeHints = psPortRangeHints,
    .PortDescriptors = c_port_descriptors,
    .instantiate = instantiate, .run = run, .cleanup = cleanup
};

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}

