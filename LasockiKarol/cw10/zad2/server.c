#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stdio.h>
#include <sys/time.h>
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

int unix_socket;
int web_socket;
int epoll;
char* unix_socket_path;

uint64_t job_id;

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
client_t clients[MAX_CLIENTS];

pthread_t pinger;
pthread_t scheduler;

void init_server(char*, char*);

void* run_scheduler(void*);

void* run_pinger(void*);

void handle_msg(int);

void send_msg(int client_id, socket_msg_t msg);

void send_empty(int, msg_type_t);

void free_client(int id);

int get_client_id(char* client_name);

socket_msg_t get_msg(int, struct sockaddr*, socklen_t*);

void free_msg(socket_msg_t msg);

void signal_handler(int);

void cleanup(void);

int main(int argc, char* argv[]) {
    if (argc != 3) exit_msg("Required 2 arguments: UDP port number and unix socket path");

    init_server(argv[1], argv[2]);
    puts("=======SERVER START========");

    struct epoll_event event;
    while (1) {
        if (epoll_wait(epoll, &event, 1, -1) == -1) exit_errno();
        handle_msg(event.data.fd);
    }
}

void init_server(char* port, char* unix_path) {
    if (atexit(cleanup) == -1) exit_errno();

    signal(SIGINT, signal_handler);

    uint16_t port_num = (uint16_t) atoi(port);
    if (port_num < 1024)
        exit_msg("Invalid port number");

    unix_socket_path = unix_path;
    if (strlen(unix_socket_path) < 1 || strlen(unix_socket_path) > UNIX_PATH_MAX)
        exit_msg("Invalid unix socket path");

    // unix socket
    struct sockaddr_un un_addr;
    un_addr.sun_family = AF_UNIX;

    snprintf(un_addr.sun_path, UNIX_PATH_MAX, "%s", unix_socket_path);

    if ((unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0)
        exit_errno();

    if (bind(unix_socket, (const struct sockaddr*) &un_addr, sizeof(un_addr)))
        exit_errno();

    // web socket
    struct sockaddr_in web_addr;
    memset(&web_addr, 0, sizeof(struct sockaddr_in));
    web_addr.sin_family = AF_INET;
    web_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    web_addr.sin_port = htons(port_num);

    if ((web_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        exit_errno();
    }

    if (bind(web_socket, (const struct sockaddr*) &web_addr, sizeof(web_addr))) {
        exit_errno();
    }



    struct epoll_event event;
    event.events = EPOLLIN | EPOLLPRI;

    if ((epoll = epoll_create1(0)) == -1)
        exit_errno();

    event.data.fd = web_socket;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, web_socket, &event) == -1)
        exit_errno();

    event.data.fd = unix_socket;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, unix_socket, &event) == -1)
        exit_errno();


    if (pthread_create(&scheduler, NULL, run_scheduler, NULL) != 0)
        exit_errno();
    if (pthread_detach(scheduler) != 0)
        exit_errno();

    if (pthread_create(&pinger, NULL, run_pinger, NULL) != 0)
        exit_errno();
    if (pthread_detach(pinger) != 0)
        exit_errno();
}

void* run_scheduler(void* arg) {
    char buffer[1024];
    while (1) {
        int min_i = MAX_CLIENTS;
        int min = 99999;

        printf("Path to file:\n");
        scanf("%1023s", buffer);

        FILE* file = fopen(buffer, "r");
        if (file == NULL) {
            fprintf(stderr, "File %s: opening error\n", buffer);
            continue;
        }
        fseek(file, 0, SEEK_END);
        size_t size = ftell(file);
        fseek(file, 0L, SEEK_SET);

        char* file_buff = malloc(size + 1);

        file_buff[size] = '\0';

        if (fread(file_buff, 1, size, file) != size) {
            fprintf(stderr, "Could not read file %s\n", buffer);
            free(file_buff);
            continue;
        }

        fclose(file);

        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!clients[i].fd) continue;
            if (min > clients[i].tasks_assigned) {
                min_i = i;
                min = clients[i].tasks_assigned;
            }
        }

        if (min_i < MAX_CLIENTS) {
            socket_msg_t msg = {WORK, strlen(file_buff) + 1, 0, ++job_id, file_buff, NULL};
            printf("Job number %lu assigned to client: %s\n", job_id, clients[min_i].name);
            send_msg(min_i, msg);
            clients[min_i].tasks_assigned++;
        } else {
            fprintf(stderr, "No clients available\n");
        }
        pthread_mutex_unlock(&client_mutex);

        free(file_buff);
    }
}

void* run_pinger(void* arg) {
    while (1) {
        pthread_mutex_lock(&client_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == 0) continue;
            if (clients[i].inactive) {
                free_client(i);
            } else {
                clients[i].inactive = 1;
                send_empty(i, PING);
            }
        }
        pthread_mutex_unlock(&client_mutex);
        sleep(10);
    }
}

void handle_msg(int socket) {
    struct sockaddr* addr = malloc(sizeof(struct sockaddr));
    if (addr == NULL) exit_errno();
    socklen_t addr_len = sizeof(struct sockaddr);
    socket_msg_t msg = get_msg(socket, addr, &addr_len);
    pthread_mutex_lock(&client_mutex);

    switch (msg.type) {
        case REGISTER: {
            msg_type_t reply = OK;
            int i;
            i = get_client_id(msg.name);
            if (i < MAX_CLIENTS)
                reply = NAME_TAKEN;

            for (i = 0; i < MAX_CLIENTS && clients[i].fd != 0; i++);

            if (i == MAX_CLIENTS)
                reply = FULL;

            if (reply != OK) {
                send_empty(socket, reply);
                break;
            }

            clients[i].fd = socket;
            clients[i].name = malloc(msg.name_size + 1);
            strcpy(clients[i].name, msg.name);
            clients[i].addr = addr;
            clients[i].addr_len = addr_len;
            clients[i].tasks_assigned = 0;
            clients[i].inactive = 0;

            send_empty(i, OK);
            break;
        }
        case UNREGISTER: {
            int i;
            for (i = 0; i < MAX_CLIENTS && (clients[i].fd == 0 || strcmp(clients[i].name, msg.name) != 0); i++);
            if (i == MAX_CLIENTS) break;
            free_client(i);
            break;
        }
        case WORK_DONE: {
            int i = get_client_id(msg.name);
            if (i < MAX_CLIENTS) {
                clients[i].inactive = 0;
                clients[i].tasks_assigned--;
            }
            printf("Job number %lu done by client: %s\n%s\n", msg.id, (char*) msg.name, (char*) msg.content);
            break;
        }
        case PONG: {
            int i = get_client_id(msg.name);
            if (i < MAX_CLIENTS)
                clients[i].inactive = 0;
        }
    }

    pthread_mutex_unlock(&client_mutex);

    free_msg(msg);
}

void free_client(int i) {
    clients[i].fd = 0;
    free(clients[i].name);
    clients[i].name = NULL;
    free(clients[i].addr);
    clients[i].addr = NULL;
    clients[i].addr_len = 0;
    clients[i].tasks_assigned = 0;
    clients[i].inactive = 0;
}

int get_client_id(char* name) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == 0) continue;
        if (strcmp(clients[i].name, name) == 0)
            break;
    }
    return i;
}

void send_msg(int client_id, socket_msg_t msg) {
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

    sendto(clients[client_id].fd, buff, size, 0, clients[client_id].addr, clients[client_id].addr_len);

    free(buff);
}

void send_empty(int client_id, msg_type_t reply) {
    socket_msg_t msg = {reply, 0, 0, 0, NULL, NULL};
    send_msg(client_id, msg);
}

socket_msg_t get_msg(int sock, struct sockaddr* addr, socklen_t* addr_len) {
    socket_msg_t msg;
    ssize_t head_size = sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size) + sizeof(msg.id);
    int8_t buff[head_size];
    if (recv(sock, buff, head_size, MSG_PEEK) < head_size)
        exit_msg("Unknown message from client");

    memcpy(&msg.type, buff, sizeof(msg.type));
    memcpy(&msg.size, buff + sizeof(msg.type), sizeof(msg.size));
    memcpy(&msg.name_size, buff + sizeof(msg.type) + sizeof(msg.size), sizeof(msg.name_size));
    memcpy(&msg.id, buff + sizeof(msg.type) + sizeof(msg.size) + sizeof(msg.name_size), sizeof(msg.id));

    ssize_t size = head_size + msg.size + 1 + msg.name_size + 1;
    int8_t* buffer = malloc(size);

    if (recvfrom(sock, buffer, size, 0, addr, addr_len) < size) {
        exit_msg("Unknown message from client");
    }

    if (msg.size > 0) {
        msg.content = malloc(msg.size + 1);
        memcpy(msg.content, buffer + head_size, msg.size + 1);
    } else {
        msg.content = NULL;
    }

    if (msg.name_size > 0) {
        msg.name = malloc(msg.name_size + 1);
        if (msg.name == NULL) exit_errno();
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

void signal_handler(int signum) {
    exit(0);
}

void cleanup(void) {
    close(web_socket);
    close(unix_socket);
    unlink(unix_socket_path);
    close(epoll);
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}