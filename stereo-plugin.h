//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

static const char * c_port_names[4] = {
    "Left In",
    "Right in",
    "Left Out",
    "Right out"
};

static LADSPA_PortDescriptor c_port_descriptors[4] = {
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT
};

static const LADSPA_PortRangeHint psPortRangeHints[4] = {
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 }
};
