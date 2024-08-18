//  SPDX-FileCopyrightText: 2016 Milan Chrobok
//  SPDX-License-Identifier: GPL-3.0-or-later

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define BROADCAST_ADDRESS "255.255.255.255"
#define BROADCAST_PORT 65432

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

void print_log(const char* LOG_ID, double l, double r) {
    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);
    fprintf(stderr, "%s %s\t%2.3f\t%2.3f\n", formattedTime, LOG_ID, l, r);
}

void file_log(char* log_dir, const char* LOG_ID, double l, double r) {
    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);

    char formattedDate[11];
    strftime(formattedDate, sizeof(formattedDate), "%Y-%m-%d", localTime);

    char path[256];
    snprintf(path, sizeof(path), "%s/%s-monitor-%s.log", log_dir,
            formattedDate, LOG_ID);
    FILE *log_file = fopen(path, "a");
    if (log_file == NULL) {
        fprintf(stderr, "%s Failed to open log file %s: %s.\n",
                formattedTime, path, strerror(errno));
        return;
    }
    fprintf(log_file, "%s\t%2.3f\t%2.3f\n", formattedTime, l, r);
    fclose(log_file);
}

void send_broadcast_message(const char *filename, double l, double r) {
    int broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_socket < 0) {
        fprintf(stderr, "Error creating broadcast socket: %s\n", strerror(errno));
        return;
    }

    int broadcast_enable = 1;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        fprintf(stderr, "Error enabling broadcast: %s\n", strerror(errno));
        close(broadcast_socket);
        return;
    }

    struct sockaddr_in broadcast_addr;
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDRESS);
    broadcast_addr.sin_port = htons(BROADCAST_PORT);

    time_t now;
    time(&now);
    struct tm *localTime = localtime(&now);
    char formattedTime[20];
    strftime(formattedTime, sizeof(formattedTime), "%Y-%m-%d %H:%M:%S",
            localTime);

    char broadcast_message[256];
    snprintf(broadcast_message, sizeof(broadcast_message),
        "%s\t%s\t%2.3f\t%2.3f\n",
        formattedTime, filename, l, r
    );

    ssize_t bytes_sent = sendto(broadcast_socket, broadcast_message, strlen(broadcast_message), 0,
                                (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    if (bytes_sent < 0) {
        fprintf(stderr, "Error sending broadcast message: %s\n", strerror(errno));
    }

    close(broadcast_socket);
}
