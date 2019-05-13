#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "chat.h"

struct Message msg;

void get_online_list();

void get_friends_list();

void respond_to_add();

void respond_to_del();

int server_queue;
int clients[MAX_CLIENTS];
int friends[MAX_CLIENTS][MAX_CLIENTS];

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}

void log_message() {
    printf("Type: %ld\n", msg.type);
    printf("Sender ID: %d\n", msg.sender_id);
    printf("Int value: %d\n", msg.int_val);
    printf("Argument: %s\n\n", msg.argument);
}

void get_timestamp(char* text, int client_id) {
    char buff[MAX_ARG_LEN];
    strncpy(buff, text, MAX_ARG_LEN);

    time_t t;
    time(&t);

    char time_string[50];
    strftime(time_string, 50, "%Y-%m-%d %H:%M:%S", localtime(&t));

    sprintf(text, "[%s] New message from %d:\n%s\n", time_string, client_id, buff);
}

void remove_all_client_friends(int client_id) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        friends[client_id][i] = -1;
}

void respond_to_init() {
    int i;
    for (i = 0; i < MAX_CLIENTS && clients[i] != 0; i++) {}

    if (i == MAX_CLIENTS)
        exit_msg("Client limit reached, server crashed");

    int client_queue_id = msg.int_val;
    clients[i] = client_queue_id;
    msg.sender_id = i;

    msgsnd(client_queue_id, &msg, MESSAGE_SIZE, 0);
}

void respond_to_stop() {
    int client_queue_id = msg.int_val;
    msg.type = STOP;
    int i;
    for (i = 0; i < MAX_CLIENTS && clients[i] != client_queue_id; i++) {}
    clients[i] = 0;
    remove_all_client_friends(i);
    msgsnd(client_queue_id, &msg, MESSAGE_SIZE, 0);
}

void respond_to_echo() {
    int client_queue_id = clients[msg.sender_id];
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    msgsnd(client_queue_id, &msg, MESSAGE_SIZE, 0);
}

void respond_to_2All() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int qid = clients[i];
        if (!qid)
            continue;

        msgsnd(qid, &msg, MESSAGE_SIZE, 0);
    }
}

void respond_to_2Friends() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int qid = clients[friends[msg.sender_id][i]];
        if (qid == 0 || friends[msg.sender_id][i] == -1)
            continue;

        msgsnd(qid, &msg, MESSAGE_SIZE, 0);
    }
}

void respond_to_2One() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    int qid = clients[msg.int_val];
    if (qid == 0)
        return;

    msgsnd(qid, &msg, MESSAGE_SIZE, 0);
}

void respond_to_list() {
    int qid = clients[msg.sender_id];
    msg.type = LIST;

    get_online_list();
    get_friends_list();

    msgsnd(qid, &msg, MESSAGE_SIZE, 0);
}

void get_friends_list() {
    const int max_id_len = 20;
    strcat(msg.argument, "Friends:\n");

    for (int i = 0; i < MAX_CLIENTS && friends[msg.sender_id][i] != -1; i++) {
        char str[max_id_len];
        snprintf(str, max_id_len, "%d", friends[msg.sender_id][i]);
        strncat(msg.argument, str, max_id_len);
        strcat(msg.argument, "\n");
    }
}

void get_online_list() {
    const int max_id_len = 20;
    memset(msg.argument, 0, sizeof(msg.argument));
    strcat(msg.argument, "Online:\n");

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != 0) {
            char str[max_id_len];
            sprintf(str, "%d", i);
            strncat(msg.argument, str, max_id_len);
        }

        if (i == msg.sender_id) strcat(msg.argument, " (You)");
        if (clients[i] != 0) strcat(msg.argument, "\n");
    }
}


void respond_to_del() {
    char* token = strtok(msg.argument, " ");

    while (token != NULL) {
        for (int i = 0; i < MAX_CLIENTS && friends[msg.sender_id][i] != -1; i++) {
            if (friends[msg.sender_id][i] == atoi(token)) {
                friends[msg.sender_id][i] = -1;
                token = strtok(NULL, " ");
                int j = i + 1;
                //push friends one place back
                while (friends[msg.sender_id][j] != -1) {
                    friends[msg.sender_id][j - 1] = friends[msg.sender_id][j];
                    friends[msg.sender_id][j] = -1;
                    j++;
                }
            }
        }
    }
}

void respond_to_add() {
    char* token = strtok(msg.argument, " ");
    while (token != NULL) {
        int already_friends = 0;
        for (int i = 0; i < MAX_CLIENTS; i++)
            if (friends[msg.sender_id][i] == atoi(token))
                already_friends = 1;

        if (!already_friends) {
            int i;
            for (i = 0; i < MAX_CLIENTS && friends[msg.sender_id][i] != -1; i++) {}
            friends[msg.sender_id][i] = atoi(token);
        }
        token = strtok(NULL, " ");
    }
}


void respond_to_friends() {
    int client_queue_id = clients[msg.sender_id];

    char* token = strtok(msg.argument, " ");
    for (int i = 0; i < MAX_CLIENTS; i++)
        friends[msg.sender_id][i] = -1;

    int i = 0;
    while (token != NULL && i < MAX_CLIENTS) {
        friends[msg.sender_id][i++] = atoi(token);
        token = strtok(NULL, " ");
    }
    msgsnd(client_queue_id, &msg, MESSAGE_SIZE, 0);
}

void exit_handler() {
    msg.type = STOP;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0)
            continue;
        msgsnd(clients[i], &msg, MESSAGE_SIZE, 0);
    }

    msgctl(server_queue, IPC_RMID, NULL);
    exit(0);
}

int main(int argc, char** argv) {
    memset(clients, 0, sizeof(clients));
    for (int i = 0; i < MAX_CLIENTS; i++)
        for (int j = 0; j < MAX_CLIENTS; j++)
            friends[i][j] = -1;

    key_t key = ftok(SERVER_ID_PATH, SERVER_ID_SEED);
    server_queue = msgget(key, IPC_CREAT | QUEUE_PERMISSIONS);

    signal(SIGINT, exit_handler);

    printf("________________________________\n\tChat server start\n________________________________\n");
    printf("Server message queue ID: %d\n", server_queue);

    while (1) {
        msgrcv(server_queue, &msg, MESSAGE_SIZE, PRIORITY_QUEUE, 0);

        log_message();

        switch (msg.type) {
            case INIT:
                respond_to_init();
                break;
            case STOP:
                respond_to_stop();
                break;
            case LIST:
                respond_to_list();
                break;
            case FRIENDS:
                respond_to_friends();
                break;
            case ECHO:
                respond_to_echo();
                break;
            case TO_ONE:
                respond_to_2One();
                break;
            case TO_ALL:
                respond_to_2All();
                break;
            case TO_FRIENDS:
                respond_to_2Friends();
                break;
            case ADD:
                respond_to_add();
                break;
            case DEL:
                respond_to_del();
                break;

            default:
                fprintf(stderr, "Command with id %ld is unrecognised \n", msg.type);
                break;
        }
    }

    return 0;
}
