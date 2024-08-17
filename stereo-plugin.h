//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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

void print_log(double l, double r) {
    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);
    fprintf(stderr, "%s %2.3f\t%2.3f\n", formattedTime, l, r);
}

void file_log(char* log_dir, const char* log_filename, double l, double r) {
    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);

    char formattedDate[11];
    strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", localTime);

    char path[256];
    snprintf(path, sizeof(path), "%s/%s-%s", log_dir,
            formattedDate, log_filename);
    FILE *log_file = fopen(path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "%s Failed to open log file %s: %s.\n",
                formattedTime, path, strerror(errno));
        return;
    }
    fprintf(log_file, "%s\t%2.3f\t%2.3f\n", formattedTime, l, r);
    fclose(log_file);
}
