//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later
#include <ladspa.h>
#include "rms-monitor-plugin.c"

// use look ahead window, 1 = precise (delayed), 0 = instant (no delay)
const int LOOK_AHEAD = 1;
const char *LOG_ID = "rms-out";

const static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b404,
    .Label = "rms_monitor_out", .Name = "RMS Monitor Out",
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

