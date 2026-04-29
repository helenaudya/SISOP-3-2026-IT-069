#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

#include "protocol.h"

void trim_newline(char *str) {
    str[strcspn(str, "\n")] = '\0';
}

void send_text(int socket_fd, const char *message) {
    send(socket_fd, message, strlen(message), 0);
}

void write_history(const char *actor, const char *message) {
    FILE *fp = fopen("history.log", "a");
    if (fp == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    fprintf(fp,
            "[%04d-%02d-%02d %02d:%02d:%02d] [%s] [%s]\n",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec,
            actor,
            message);

    fclose(fp);
}
