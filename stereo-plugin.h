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
#include <pthread.h>

#define BROADCAST_ADDRESS "255.255.255.255"
#define BROADCAST_PORT 65432

static const char * c_port_names[5] = {
    "Left In",
    "Right In",
    "Left Out",
    "Right Out",
    "Input Gain"
};

static LADSPA_PortDescriptor c_port_descriptors[5] = {
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_INPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_AUDIO | LADSPA_PORT_OUTPUT,
    LADSPA_PORT_CONTROL | LADSPA_PORT_INPUT
};

static const LADSPA_PortRangeHint psPortRangeHints[5] = {
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = 0, .LowerBound = 0, .UpperBound = 0 },
    { .HintDescriptor = LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, .LowerBound = -24.0, .UpperBound = 24.0 }
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

static int broadcast_socket = -1;
static struct sockaddr_in broadcast_addr;
static pthread_mutex_t broadcast_mutex = PTHREAD_MUTEX_INITIALIZER;

void setup_socket() {
    if (broadcast_socket >= 0) return;
    pthread_mutex_lock(&broadcast_mutex);
    broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_socket < 0) {
        fprintf(stderr, "Error creating broadcast socket: %s\n", strerror(errno));
        broadcast_socket = -1;
        pthread_mutex_unlock(&broadcast_mutex);
        return;
    }

    int broadcast_enable = 1;
    if (setsockopt(broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        fprintf(stderr, "Error enabling broadcast: %s\n", strerror(errno));
        close(broadcast_socket);
        broadcast_socket = -1;
        pthread_mutex_unlock(&broadcast_mutex);
        return;
    }
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDRESS);
    pthread_mutex_unlock(&broadcast_mutex);
}

void send_broadcast_message(const char *filename, double l, double r) {
    if (broadcast_socket < 0) return;
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

    pthread_mutex_lock(&broadcast_mutex);
    ssize_t bytes_sent = sendto(broadcast_socket, broadcast_message, strlen(broadcast_message), MSG_DONTWAIT,
                                (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    pthread_mutex_unlock(&broadcast_mutex);
    if (bytes_sent < 0) fprintf(stderr, "Error sending broadcast message: %s\n", strerror(errno));
}

void close_socket() {
    if (broadcast_socket < 0) return;
    pthread_mutex_lock(&broadcast_mutex);
    close(broadcast_socket);
    broadcast_socket = -1;
    pthread_mutex_unlock(&broadcast_mutex);
}
