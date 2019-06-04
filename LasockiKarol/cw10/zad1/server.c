#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
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

void* run_pinger(void*);

void* run_scheduler(void*);

void register_client(int sock);

void handle_msg(int sock);

void send_msg(int client_id, socket_msg_t msg);

void send_empty(int, msg_type_t);

void free_client(int id);

void free_socket(int socket);

int get_client_id(char* client_name);

socket_msg_t get_msg(int socket);

void free_msg(socket_msg_t msg);

void signal_handler(int);

void cleanup(void);

int main(int argc, char* argv[]) {
    if (argc != 3) exit_msg("Required 2 arguments: TCP port number and unix socket path");

    init_server(argv[1], argv[2]);
    puts("=======SERVER START========");

    struct epoll_event event;
    while (1) {
        if (epoll_wait(epoll, &event, 1, -1) == -1) exit_errno();

        if (event.data.fd < 0)
            register_client(-event.data.fd);
        else
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
        exit_msg("Invalid unix socket path length");

    //unix socket
    struct sockaddr_un un_addr;
    un_addr.sun_family = AF_UNIX;

    snprintf(un_addr.sun_path, UNIX_PATH_MAX, "%s", unix_socket_path);

    if ((unix_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
        exit_errno();

    if (bind(unix_socket, (const struct sockaddr*) &un_addr, sizeof(un_addr)))
        exit_errno();

    if (listen(unix_socket, MAX_CLIENTS) == -1)
        exit_errno();

    //web socket
    struct sockaddr_in web_addr;
    memset(&web_addr, 0, sizeof(struct sockaddr_in));
    web_addr.sin_family = AF_INET;
    web_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    web_addr.sin_port = htons(port_num);

    if ((web_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        exit_errno();
    }

    if (bind(web_socket, (const struct sockaddr*) &web_addr, sizeof(web_addr))) {
        exit_errno();
    }

    if (listen(web_socket, 64) == -1) {
        exit_errno();
    }


    struct epoll_event event;
    event.events = EPOLLIN | EPOLLPRI;

    if ((epoll = epoll_create1(0)) == -1)
        exit_errno();

    event.data.fd = -web_socket;
    if (epoll_ctl(epoll, EPOLL_CTL_ADD, web_socket, &event) == -1)
        exit_errno();

    event.data.fd = -unix_socket;
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
        // get file size
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
            send_msg(clients[min_i].fd, msg);
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
                send_empty(clients[i].fd, PING);
            }
        }
        pthread_mutex_unlock(&client_mutex);
        sleep(5);
    }
}

void register_client(int sock) {
    puts("New client connecting...");
    int client = accept(sock, NULL, NULL);
    if (client == -1) exit_errno();
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLPRI;
    event.data.fd = client;

    if (epoll_ctl(epoll, EPOLL_CTL_ADD, client, &event) == -1)
        exit_errno();
}

void handle_msg(int sock) {
    socket_msg_t msg = get_msg(sock);
    pthread_mutex_lock(&client_mutex);

    switch (msg.type) {
        case REGISTER: {
            msg_type_t reply = OK;
            int i = get_client_id(msg.name);
            if (i < MAX_CLIENTS)
                reply = NAME_TAKEN;

            for (i = 0; i < MAX_CLIENTS && clients[i].fd != 0; i++);

            if (i == MAX_CLIENTS)
                reply = FULL;

            if (reply != OK) {
                send_empty(sock, reply);
                free_socket(sock);
                break;
            }

            clients[i].fd = sock;
            clients[i].name = malloc(msg.size + 1);
            strcpy(clients[i].name, msg.name);
            clients[i].tasks_assigned = 0;
            clients[i].inactive = 0;

            send_empty(sock, OK);
            break;
        }
        case UNREGISTER: {
            int i;
            for (i = 0; i < MAX_CLIENTS && strcmp(clients[i].name, msg.name) != 0; i++);
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

void free_client(int id) {
    free_socket(clients[id].fd);
    clients[id].fd = 0;
    clients[id].name = NULL;
    clients[id].tasks_assigned = 0;
    clients[id].inactive = 0;
}

int get_client_id(char* client_name) {
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == 0) continue;
        if (strcmp(clients[i].name, client_name) == 0)
            break;
    }
    return i;
}

void free_socket(int socket) {
    epoll_ctl(epoll, EPOLL_CTL_DEL, socket, NULL);
    shutdown(socket, SHUT_RDWR);
    close(socket);
}

void send_msg(int socket, socket_msg_t msg) {
    write(socket, &msg.type, sizeof(msg.type));
    write(socket, &msg.size, sizeof(msg.size));
    write(socket, &msg.name_size, sizeof(msg.name_size));
    write(socket, &msg.id, sizeof(msg.id));
    if (msg.size > 0) write(socket, msg.content, msg.size);
    if (msg.name_size > 0) write(socket, msg.name, msg.name_size);
}

void send_empty(int socket, msg_type_t reply) {
    socket_msg_t msg = {reply, 0, 0, 0, NULL, NULL};
    send_msg(socket, msg);
}

socket_msg_t get_msg(int socket) {
    socket_msg_t msg;
    if (read(socket, &msg.type, sizeof(msg.type)) != sizeof(msg.type))
        exit_msg("Unknown message from client");
    if (read(socket, &msg.size, sizeof(msg.size)) != sizeof(msg.size))
        exit_msg("Unknown message from client");
    if (read(socket, &msg.name_size, sizeof(msg.name_size)) != sizeof(msg.name_size))
        exit_msg("Unknown message from client");
    if (read(socket, &msg.id, sizeof(msg.id)) != sizeof(msg.id))
        exit_msg("Unknown message from client");
    if (msg.size > 0) {
        msg.content = malloc(msg.size + 1);
        if (read(socket, msg.content, msg.size) != msg.size) {
            exit_msg("Unknown message from client");
        }
    } else {
        msg.content = NULL;
    }
    if (msg.name_size > 0) {
        msg.name = malloc(msg.name_size + 1);
        if (read(socket, msg.name, msg.name_size) != msg.name_size) {
            exit_msg("Unknown message from client");
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