#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#include "protocol.h"

typedef struct {
    int socket_fd;
    char name[NAME_SIZE];
    int type;
} Client;

Client clients[MAX_CLIENTS];
int server_socket;
time_t server_start;

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].socket_fd = 0;
        clients[i].name[0] = '\0';
        clients[i].type = TYPE_NORMAL;
    }
}

int is_name_used(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd != 0 &&
            strcmp(clients[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

int add_client(int socket_fd, const char *name, int type) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd == 0) {
            clients[i].socket_fd = socket_fd;
            strncpy(clients[i].name, name, NAME_SIZE - 1);
            clients[i].name[NAME_SIZE - 1] = '\0';
            clients[i].type = type;
            return i;
        }
    }
    return -1;
}

void remove_client(int index) {
    char log_msg[BUFFER_SIZE];

    if (clients[index].socket_fd != 0) {
        snprintf(log_msg, sizeof(log_msg),
                 "User '%s' disconnected",
                 clients[index].name);

        write_history("System", log_msg);

        close(clients[index].socket_fd);
        clients[index].socket_fd = 0;
        clients[index].name[0] = '\0';
        clients[index].type = TYPE_NORMAL;
    }
}

void broadcast_chat(int sender_index, const char *chat) {
    char message[BUFFER_SIZE];
    char log_msg[BUFFER_SIZE];

    snprintf(message, sizeof(message),
             "[%s]: %s\n",
             clients[sender_index].name,
             chat);

    snprintf(log_msg, sizeof(log_msg),
             "[%s]: %s",
             clients[sender_index].name,
             chat);

    write_history("User", log_msg);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd != 0 &&
            i != sender_index &&
            clients[i].type == TYPE_NORMAL) {
            send_text(clients[i].socket_fd, message);
        }
    }
}

void send_admin_menu(int socket_fd) {
    send_text(socket_fd,
        "\n=== THE KNIGHTS CONSOLE ===\n"
        "1. Check Active Entities (Users)\n"
        "2. Check Server Uptime\n"
        "3. Execute Emergency Shutdown\n"
        "4. Disconnect\n"
        "Command >> "
    );
}

void shutdown_server() {
    write_history("System", "EMERGENCY SHUTDOWN INITIATED");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket_fd != 0) {
            send_text(clients[i].socket_fd,
                      "[System] Server is shutting down...\n");
            close(clients[i].socket_fd);
            clients[i].socket_fd = 0;
        }
    }

    close(server_socket);
    exit(0);
}

void handle_admin_command(int index, const char *command) {
    char response[BUFFER_SIZE];

    if (strcmp(command, "1") == 0) {
        int active_users = 0;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].socket_fd != 0 &&
                clients[i].type == TYPE_NORMAL) {
                active_users++;
            }
        }

        snprintf(response, sizeof(response),
                 "[Admin] Active Entities: %d\n",
                 active_users);

        send_text(clients[index].socket_fd, response);
        write_history("Admin", "RPC_GET_USERS");
        send_admin_menu(clients[index].socket_fd);
    }

    else if (strcmp(command, "2") == 0) {
        time_t now = time(NULL);
        int uptime = (int) difftime(now, server_start);

        snprintf(response, sizeof(response),
                 "[Admin] Server Uptime: %d seconds\n",
                 uptime);

        send_text(clients[index].socket_fd, response);
        write_history("Admin", "RPC_GET_UPTIME");
        send_admin_menu(clients[index].socket_fd);
    }

    else if (strcmp(command, "3") == 0) {
        write_history("Admin", "RPC_SHUTDOWN");
        send_text(clients[index].socket_fd,
                  "[System] Emergency shutdown initiated.\n");
        shutdown_server();
    }

    else if (strcmp(command, "4") == 0 ||
             strcmp(command, "/exit") == 0) {
        send_text(clients[index].socket_fd,
                  "[System] Disconnecting from The Wired...\n");
        remove_client(index);
    }

    else {
        send_text(clients[index].socket_fd,
                  "[Admin] Invalid command.\n");
        send_admin_menu(clients[index].socket_fd);
    }
}

void accept_new_client() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int new_socket = accept(server_socket,
                            (struct sockaddr *) &client_addr,
                            &addr_len);

    if (new_socket < 0) {
        perror("accept");
        return;
    }

    char name[NAME_SIZE];
    char password[NAME_SIZE];

    memset(name, 0, sizeof(name));
    memset(password, 0, sizeof(password));

    int received = recv(new_socket, name, sizeof(name) - 1, 0);
    if (received <= 0) {
        close(new_socket);
        return;
    }

    trim_newline(name);

    if (strlen(name) == 0) {
        send_text(new_socket, "[System] Invalid identity.\n");
        close(new_socket);
        return;
    }

    if (strcmp(name, ADMIN_NAME) == 0) {
        received = recv(new_socket, password, sizeof(password) - 1, 0);
        if (received <= 0) {
            close(new_socket);
            return;
        }

        trim_newline(password);

        if (strcmp(password, ADMIN_PASSWORD) != 0) {
            send_text(new_socket,
                      "[System] Authentication failed.\n");
            close(new_socket);
            return;
        }

        int index = add_client(new_socket, ADMIN_NAME, TYPE_ADMIN);

        if (index == -1) {
            send_text(new_socket, "[System] Server is full.\n");
            close(new_socket);
            return;
        }

        write_history("System", "User 'The Knights' connected");

        send_text(new_socket,
                  "[System] Authentication successful. Granted Admin privileges.\n");

        send_admin_menu(new_socket);
        return;
    }

    if (is_name_used(name)) {
        char reject_msg[BUFFER_SIZE];

        snprintf(reject_msg, sizeof(reject_msg),
                 "[System] The identity '%s' is already synchronized in The Wired.\n",
                 name);

        send_text(new_socket, reject_msg);
        close(new_socket);
        return;
    }

    int index = add_client(new_socket, name, TYPE_NORMAL);

    if (index == -1) {
        send_text(new_socket, "[System] Server is full.\n");
        close(new_socket);
        return;
    }

    char welcome[BUFFER_SIZE];
    char log_msg[BUFFER_SIZE];

    snprintf(welcome, sizeof(welcome),
             "--- Welcome to The Wired, %s ---\n",
             name);

    snprintf(log_msg, sizeof(log_msg),
             "User '%s' connected",
             name);

    send_text(new_socket, welcome);
    write_history("System", log_msg);

    printf("[System] User '%s' connected\n", name);
}

int main() {
    struct sockaddr_in server_addr;
    fd_set readfds;

    server_start = time(NULL);
    init_clients();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(1);
    }

    int option = 1;
    setsockopt(server_socket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &option,
               sizeof(option));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket,
             (struct sockaddr *) &server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        exit(1);
    }

    printf("[System] SERVER ONLINE at %s:%d\n",
           SERVER_IP,
           SERVER_PORT);

    write_history("System", "SERVER ONLINE");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);

        int max_fd = server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int socket_fd = clients[i].socket_fd;

            if (socket_fd > 0) {
                FD_SET(socket_fd, &readfds);
            }

            if (socket_fd > max_fd) {
                max_fd = socket_fd;
            }
        }

        int activity = select(max_fd + 1,
                              &readfds,
                              NULL,
                              NULL,
                              NULL);

        if (activity < 0) {
            perror("select");
            continue;
        }

        if (FD_ISSET(server_socket, &readfds)) {
            accept_new_client();
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int socket_fd = clients[i].socket_fd;

            if (socket_fd > 0 && FD_ISSET(socket_fd, &readfds)) {
                char buffer[BUFFER_SIZE];

                memset(buffer, 0, sizeof(buffer));

                int received = recv(socket_fd,
                                    buffer,
                                    sizeof(buffer) - 1,
                                    0);

                if (received <= 0) {
                    remove_client(i);
                    continue;
                }

                trim_newline(buffer);

                if (clients[i].type == TYPE_ADMIN) {
                    handle_admin_command(i, buffer);
                } else {
                    if (strcmp(buffer, "/exit") == 0) {
                        send_text(socket_fd,
                                  "[System] Disconnecting from The Wired...\n");
                        remove_client(i);
                    } else {
                        broadcast_chat(i, buffer);
                    }
                }
            }
        }
    }

    return 0;
}
