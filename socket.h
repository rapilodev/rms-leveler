#ifndef SOCKET_H
#define SOCKET_H

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BROADCAST_ADDRESS "127.0.0.1"
#define BROADCAST_PORT 65432

static int global_broadcast_socket = -1;
static struct sockaddr_in broadcast_addr;
static pthread_mutex_t broadcast_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void setup_socket() {
    pthread_mutex_lock(&broadcast_mutex);
    if (global_broadcast_socket >= 0) {
        pthread_mutex_unlock(&broadcast_mutex);
        return;
    }

    global_broadcast_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (global_broadcast_socket < 0) {
        fprintf(stderr, "Error creating socket: %s\n", strerror(errno));
        pthread_mutex_unlock(&broadcast_mutex);
        return;
    }

    int broadcast_enable = 1;
    if (setsockopt(global_broadcast_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        fprintf(stderr, "Error enabling broadcast: %s\n", strerror(errno));
        close(global_broadcast_socket);
        global_broadcast_socket = -1;
        pthread_mutex_unlock(&broadcast_mutex);
        return;
    }

    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_ADDRESS);
    pthread_mutex_unlock(&broadcast_mutex);
}

static inline void send_broadcast_message(const char *log_id, double l, double r) {
    if (global_broadcast_socket < 0) return;
    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &lt);

    char msg[256];
    int len = snprintf(msg, sizeof(msg), "%s\t%s\t%2.3f\t%2.3f\n", time_str, log_id, l, r);

    if (len > 0) {
        sendto(global_broadcast_socket, msg, len, MSG_DONTWAIT,
               (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr));
    }
}

static inline void close_socket() {
    pthread_mutex_lock(&broadcast_mutex);
    if (global_broadcast_socket >= 0) {
        close(global_broadcast_socket);
        global_broadcast_socket = -1;
    }
    pthread_mutex_unlock(&broadcast_mutex);
}

#endif
