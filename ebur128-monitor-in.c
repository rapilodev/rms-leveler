//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <ladspa.h>
#include "ebur-monitor-plugin.c"

// use look ahead window, 1 = precise (delayed), 0 = instant (no delay)
const int LOOK_AHEAD = 1;
const char *LOG_ID = "lufs-in";

const static LADSPA_Descriptor mon_ladspa_descriptor = { .UniqueID = 0x22b305,
    .Label = "ebur128_monitor_in", .Name = "EBU R128 Monitor In",
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
    if (i == 0) return &mon_ladspa_descriptor;
    return 0;
}
