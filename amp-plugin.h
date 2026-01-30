//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#ifndef amp_plugin_h
#define amp_plugin_h

#include <ladspa.h>

#define ARRAY_LENGTH(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char* amp_port_names[5] = {
    "Left In",
    "Right In",
    "Left Out",
    "Right Out",
    "Input Gain"
};

static const LADSPA_PortDescriptor amp_port_descriptors[5] = {
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_INPUT  | LADSPA_PORT_CONTROL,
};

static const LADSPA_PortRangeHint amp_port_range_hints[5] = {
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, .LowerBound = -24.0, .UpperBound = 24.0 }
};

#endif
