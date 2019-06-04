#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>

void exit_errno();

void exit_msg(char* msg);

#define UNIX_PATH_MAX 108
#define MAX_CLIENTS 7

typedef enum msg_type_t {
    OK,
    REGISTER,
    UNREGISTER,
    NAME_TAKEN,
    WORK,
    WORK_DONE,
    FULL,
    PING,
    PONG,
} msg_type_t;

typedef struct socket_msg_t {
    uint8_t type;
    uint64_t size;
    uint64_t name_size;
    uint64_t id;
    void* content;
    char* name;
} socket_msg_t;

typedef struct client_t {
    int fd;
    char* name;
    struct sockaddr* addr;
    socklen_t addr_len;
    int tasks_assigned;
    int inactive;
} client_t;

int sock;
char* name;

void init_client(char*, char*, char*);

socket_msg_t get_msg(void);

void free_msg(socket_msg_t msg);

void send_msg(socket_msg_t);

void send_empty(msg_type_t);

void send_done(int, char*);

void signal_handler(int);

void cleanup(void);

int main(int argc, char* argv[]) {
    if (argc != 4) exit_msg("Required 3 arguments: client name, communication type (WEB/UNIX), socket address (IP or unix path)");

    init_client(argv[1], argv[2], argv[3]);

    while (1) {
        socket_msg_t msg = get_msg();

        switch (msg.type) {
            case OK: {
                break;
            }
            case PING: {
                send_empty(PONG);
                break;
            }
            case NAME_TAKEN:
                exit_msg("Name is already taken");
            case FULL:
                exit_msg("Server is full");
            case WORK: {
                puts("Starting calculations...");
                char *buffer = malloc(100 + 2 * msg.size);
                sprintf(buffer, "echo '%s' | awk '{for(x=1;$x;++x)print $x}' | sort | uniq -c", (char*) msg.content);
                FILE *result = popen(buffer, "r");
                if (result == 0) { free(buffer); break; }
                int n = fread(buffer, 1, 99 + 2 * msg.size, result);
                buffer[n] = '\0';
                puts("Finished");
                send_done(msg.id, buffer);
                free(buffer);
                break;
            }
            default:
                break;
        }

        free_msg(msg);
    }
}

void init_client(char* n, char* type, char* address) {
    if (atexit(cleanup) == -1) exit_errno();

    signal(SIGINT, signal_handler);

    name = n;

    if (strcmp("WEB", type) == 0) {
        strtok(address, ":");
        char* port = strtok(NULL, ":");
        if (port == NULL) exit_msg("Specify a port");

        uint32_t in_addr = inet_addr(address);
        if (in_addr == INADDR_NONE) exit_msg("Invalid address");

        uint16_t port_num = (uint16_t) atoi(port);
        if (port_num < 1024)
            exit_msg("Invalid port number");

        if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
            exit_errno();

        struct sockaddr_in web_addr;
        memset(&web_addr, 0, sizeof(struct sockaddr_in));

        web_addr.sin_family = AF_INET;
        web_addr.sin_addr.s_addr = in_addr;
        web_addr.sin_port = htons(port_num);

        if (connect(sock, (const struct sockaddr*) &web_addr, sizeof(web_addr)) == -1)
            exit_errno();
    } else if (strcmp("UNIX", type) == 0) {
        char* unix_socket_path = address;

        if (strlen(unix_socket_path) < 1 || strlen(unix_socket_path) > UNIX_PATH_MAX)
            exit_msg("Invalid unix socket path");

        struct sockaddr_un unix_addr;
        unix_addr.sun_family = AF_UNIX;
        snprintf(unix_addr.sun_path, UNIX_PATH_MAX, "%s", unix_socket_path);

        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        snprintf(client_addr.sun_path, UNIX_PATH_MAX, "%s", name);

        if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1)
            exit_errno();

        if (bind(sock, (const struct sockaddr*) &client_addr, sizeof(client_addr)) == -1)
            exit_errno();

        if (connect(sock, (const struct sockaddr*) &unix_addr, sizeof(unix_addr)) == -1)
            exit_errno();
    } else {
        exit_msg("Unknown connection type");
    }

    send_empty(REGISTER);
}

void send_msg(socket_msg_t msg) {
    ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
    ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
    int8_t* buff = malloc(size);
    if (buff == NULL) exit_errno();

    memcpy(buff, &msg.type, sizeof(msg.type));
    memcpy(buff + sizeof(msg.type), &msg.size, sizeof(msg.size));
    memcpy(buff + sizeof(msg.type) + sizeof(msg.size), &msg.name_size, sizeof(msg.name_size));
    memcpy(buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), &msg.id, sizeof(msg.id));

    if (msg.size > 0 && msg.content != NULL)
        memcpy(buff + head_size, msg.content, msg.size + 1);
    if (msg.name_size > 0 && msg.name != NULL)
        memcpy(buff + head_size + msg.size + 1, msg.name, msg.name_size + 1);

    if (write(sock, buff, size) != size) exit_errno();

    free(buff);
}

void send_empty(msg_type_t type) {
    socket_msg_t msg = {type, 0, strlen(name), 0, NULL, name};
    send_msg(msg);
};

void send_done(int id, char* content) {
    socket_msg_t msg = {WORK_DONE, strlen(content), strlen(name), id, content, name};
    send_msg(msg);
}

socket_msg_t get_msg(void) {
    socket_msg_t msg;
    ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
    int8_t buff[head_size];
    if (recv(sock, buff, head_size, MSG_PEEK) < head_size)
        exit_msg("Unknown message from server");

    memcpy(&msg.type, buff, sizeof(msg.type));
    memcpy(&msg.size, buff + sizeof(msg.type), sizeof(msg.size));
    memcpy(&msg.name_size, buff + sizeof(msg.type) + sizeof(msg.size), sizeof(msg.name_size));
    memcpy(&msg.id, buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), sizeof(msg.id));

    ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
    int8_t* buffer = malloc(size);

    if (recv(sock, buffer, size, 0) < size) {
        exit_msg("Unknown message from server");
    }

    if (msg.size > 0) {
        msg.content = malloc(msg.size + 1);
        memcpy(msg.content, buffer + head_size, msg.size + 1);
    } else {
        msg.content = NULL;
    }

    if (msg.name_size > 0) {
        msg.name = malloc(msg.name_size + 1);
        memcpy(msg.name, buffer + head_size + msg.size + 1, msg.name_size + 1);
    } else {
        msg.name = NULL;
    }

    free(buffer);

    return msg;
}

void free_msg(socket_msg_t msg) {
    if (msg.content != NULL)
        free(msg.content);
    if (msg.name != NULL)
        free(msg.name);
}

void signal_handler(int signo) {
    exit(0);
}

void cleanup(void) {
    send_empty(UNREGISTER);
    unlink(name);
    shutdown(sock, SHUT_RDWR);
    close(sock);
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}