#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "protocol.h"

int client_socket;

void handle_sigint(int sig) {
    send_text(client_socket, "/exit");
    printf("\n[System] Disconnecting from The Wired...\n");
    close(client_socket);
    exit(0);
}

int main() {
    struct sockaddr_in server_addr;
    fd_set readfds;

    char name[NAME_SIZE];
    char password[NAME_SIZE];
    char buffer[BUFFER_SIZE];

    signal(SIGINT, handle_sigint);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);

    if (client_socket < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (connect(client_socket,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect");
        close(client_socket);
        exit(1);
    }

    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    trim_newline(name);

    send_text(client_socket, name);

    if (strcmp(name, ADMIN_NAME) == 0) {
        usleep(100000);

        printf("Enter Password: ");
        fgets(password, sizeof(password), stdin);
        trim_newline(password);

        send_text(client_socket, password);
    }

    while (1) {
        FD_ZERO(&readfds);

        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(client_socket, &readfds);

        int max_fd = client_socket;

        int activity = select(max_fd + 1,
                              &readfds,
                              NULL,
                              NULL,
                              NULL);

        if (activity < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(client_socket, &readfds)) {
            memset(buffer, 0, sizeof(buffer));

            int received = recv(client_socket,
                                buffer,
                                sizeof(buffer) - 1,
                                0);

            if (received <= 0) {
                printf("[System] Server disconnected.\n");
                break;
            }

            printf("%s", buffer);
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, sizeof(buffer));

            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                continue;
            }

            trim_newline(buffer);

            if (strlen(buffer) == 0) {
                continue;
            }

            send_text(client_socket, buffer);

            if (strcmp(buffer, "/exit") == 0) {
                printf("[System] Disconnecting from The Wired...\n");
                break;
            }
        }
    }

    close(client_socket);
    return 0;
}
