#ifndef PROTOCOL_H
#define PROTOCOL_H

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define NAME_SIZE 50

#define ADMIN_NAME "The Knights"
#define ADMIN_PASSWORD "berrybush"

#define TYPE_NORMAL 0
#define TYPE_ADMIN 1

void trim_newline(char *str);
void write_history(const char *actor, const char *message);
void send_text(int socket_fd, const char *message);

#endif
