//  SPDX-FileCopyrightText: 2024 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include "rms-monitor-plugin.c"

// long term measurement window
const double BUFFER_DURATION1 = 6.0;
// use look ahead window, 1 = precise (delayed), 0 = instant (no delay)
const int LOOK_AHEAD = 1;
const char *LOG_ID = "rms-out";

static LADSPA_Descriptor c_ladspa_descriptor = { .UniqueID = 0x22b404,
    .Label = "rms_monitor_out_6s", .Name = "RMS monitor out, 6 seconds window",
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

