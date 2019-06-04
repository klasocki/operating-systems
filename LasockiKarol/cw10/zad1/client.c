#include <pthread.h>
#include <signal.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/types.h>
#include <stdint.h>
#include <sys/socket.h>

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

void send_msg(socket_msg_t*);

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
                if(buffer == NULL) exit_errno();
                sprintf(buffer, "echo '%s' | awk '{for(x=1;$x;++x)print $x}' | sort | uniq -c", (char*) msg.content);

                FILE *result = popen(buffer, "r");
                if (result == NULL) { free(buffer); break; }
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

        struct sockaddr_in web_addr;
        memset(&web_addr, 0, sizeof(struct sockaddr_in));

        web_addr.sin_family = AF_INET;
        web_addr.sin_addr.s_addr = in_addr;
        web_addr.sin_port = htons(port_num);

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1)
            exit_errno();

        if (connect(sock, (const struct sockaddr*) &web_addr, sizeof(web_addr)) == -1)
            exit_errno();
    } else if (strcmp("UNIX", type) == 0) {
        char* unix_socket_path = address;

        if (strlen(unix_socket_path) < 1 || strlen(unix_socket_path) > UNIX_PATH_MAX)
            exit_msg("Invalid unix socket path");

        struct sockaddr_un unix_addr;
        unix_addr.sun_family = AF_UNIX;
        snprintf(unix_addr.sun_path, UNIX_PATH_MAX, "%s", unix_socket_path);

        if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
            exit_errno();

        if (connect(sock, (const struct sockaddr*) &unix_addr, sizeof(unix_addr)) == -1)
            exit_errno();
    } else {
        exit_msg("Unknown connection type");
    }

    send_empty(REGISTER);
}

void send_msg(socket_msg_t* msg) {
    write(sock, &msg->type, sizeof(msg->type));
    write(sock, &msg->size, sizeof(msg->size));
    write(sock, &msg->name_size, sizeof(msg->name_size));
    write(sock, &msg->id, sizeof(msg->id));
    if (msg->size > 0) write(sock, msg->content, msg->size);
    if (msg->name_size > 0) write(sock, msg->name, msg->name_size);
}

void send_empty(msg_type_t type) {
    socket_msg_t msg = {type, 0, strlen(name) + 1, 0, NULL, name};
    send_msg(&msg);
}

void send_done(int id, char* content) {
    socket_msg_t msg = {WORK_DONE, strlen(content) + 1, strlen(name) + 1, id, content, name};
    send_msg(&msg);
}

socket_msg_t get_msg(void) {
    socket_msg_t msg;
    if (read(sock, &msg.type, sizeof(msg.type)) != sizeof(msg.type))
        exit_msg("Unknown message from server");
    if (read(sock, &msg.size, sizeof(msg.size)) != sizeof(msg.size))
        exit_msg("Unknown message from server");
    if (read(sock, &msg.name_size, sizeof(msg.name_size)) != sizeof(msg.name_size))
        exit_msg("Unknown message from server");
    if (read(sock, &msg.id, sizeof(msg.id)) != sizeof(msg.id))
        exit_msg("Unknown message from server");
    if (msg.size > 0) {
        msg.content = malloc(msg.size + 1);
        if (read(sock, msg.content, msg.size) != msg.size)
            exit_msg("Unknown message from server");
    } else {
        msg.content = NULL;
    }
    if (msg.name_size > 0) {
        msg.name = malloc(msg.name_size + 1);
        if (read(sock, msg.name, msg.name_size) != msg.name_size) {
            exit_msg("Unknown message from server");
        }
    } else {
        msg.name = NULL;
    }
    return msg;
}

void free_msg(socket_msg_t msg) {
    if (msg.content != NULL)
        free(msg.content);
    if (msg.name != NULL)
        free(msg.name);
}

void signal_handler(int signum) {
    exit(0);
}

void cleanup(void) {
    send_empty(UNREGISTER);
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