//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef monitor_plugin
#define monitor_plugin
#include <ladspa.h>

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define WINDOW_MIN 0.0
#define WINDOW_INIT 6.0
#define WINDOW_MAX 24.0
#define LOG_INTERVAL 5.0
// ^ LADSPA_HINT_DEFAULT_LOW should set 25% of MAX (24) = INIT (6)

static const char* mon_port_names[5] = {
    "Left In",
    "Right In",
    "Left Out",
    "Right Out",
    "Window"
};

static const LADSPA_PortDescriptor mon_port_descriptors[5] = {
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL
};

static const LADSPA_PortRangeHint mon_port_range_hints[5] = {
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_LOW, .LowerBound = WINDOW_MIN, .UpperBound = WINDOW_MAX }
};

#endif
