//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef peak_monitor_out
#define peak_monitor_out

#include <ladspa.h>
#include "peak-monitor-plugin.c"

// use look ahead window, 1 = precise (delayed), 0 = instant (no delay)
const int LOOK_AHEAD = 1;
const char *LOG_ID = "peak-out";

const static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b308,
    .Label = "peak_monitor_out", .Name = "PEAK Monitor Out",
    .Maker = "Milan Chrobok", .Copyright = "GPL 3",
    .PortCount = 5,
    .PortNames = mon_port_names,
    .PortRangeHints = mon_port_range_hints,
    .PortDescriptors = mon_port_descriptors,
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
