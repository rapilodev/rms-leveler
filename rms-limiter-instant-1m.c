//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef rms_limiter_instant_1m
#define rms_limiter_instant_1m

#include <ladspa.h>
#include "single-window-plugin.c"

// set 1 for leveler or 0 for limiter
const int IS_LEVELER = 0;
// long term measurement window
const double BUFFER_DURATION1 = 60.0;
// use look ahead window, 1 = precise (delayed), 0 = instant (no delay)
const int LOOK_AHEAD = 0;

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b411,
    .Label = "rms_limiter_instant_1m", .Name = "RMS limiter -20dBFS, 1 minute window",
    .Maker = "Milan Chrobok", .Copyright = "GPL 3",
    .PortCount = 5,
    .PortNames = amp_port_names,
    .PortRangeHints = amp_port_range_hints,
    .PortDescriptors = amp_port_descriptors,
    .connect_port = connect_port,
    .instantiate = instantiate,
    .run = run,
    .cleanup = cleanup
};

const LADSPA_Descriptor * ladspa_descriptor(unsigned long i) {
    if (i == 0) return &c_ladspa_descriptor;
    return 0;
}

#endif
