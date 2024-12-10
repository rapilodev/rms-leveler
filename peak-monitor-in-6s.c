//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "peak-monitor-plugin.c"

// long term measurement window
const double BUFFER_DURATION1 = 6.0;
const char *LOG_ID = "peak-in";

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b307,
    .Label = "peak_monitor_in_6s", .Name = "peak monitor in, 6 seconds window",
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
