//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "ebur-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 0;
// long term measurement window
const double BUFFER_DURATION1 = 3.0;

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b303,
    .Label = "ebur128_limiter_3s", .Name = "EBU R128 limiter -20dBFS, 3 seconds window",
    .Maker = "Milan Chrobok", .Copyright = "GPL 3",
    .PortCount = 4, .connect_port = connect_port,
    .PortNames = c_port_names, .PortRangeHints = psPortRangeHints,
    .PortDescriptors = c_port_descriptors,
    .instantiate = instantiate, .run = run, .cleanup = cleanup
};

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}
