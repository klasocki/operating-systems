#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <mqueue.h>
#include "chat.h"

struct Message msg;

void get_online_list();

void get_friends_list();

void respond_to_add();

void respond_to_del();

mqd_t server_queue;
mqd_t clients[MAX_CLIENTS];
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

    char* client_queue_name = msg.argument;
    mqd_t client_queue = mq_open(client_queue_name, O_WRONLY);
    clients[i] = client_queue;
    msg.sender_id = i;

    mq_send(client_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);
}

void respond_to_stop() {
    mqd_t client_queue = msg.int_val;
    msg.type = STOP;
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i] == client_queue) clients[i] = 0;

    remove_all_client_friends(msg.sender_id);
    mq_send(client_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);
    mq_close(client_queue);
}

void respond_to_echo() {
    mqd_t client_queue = clients[msg.sender_id];
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    mq_send(client_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);
}

void respond_to_2All() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        mqd_t client_queue = clients[i];
        if (!client_queue)
            continue;

        mq_send(client_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);
    }
}

void respond_to_2Friends() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        mqd_t qid = clients[friends[msg.sender_id][i]];
        if (!qid || friends[msg.sender_id][i] == -1)
            continue;

        mq_send(qid, (const char*) &msg, MESSAGE_SIZE, msg.type);
    }
}

void respond_to_2One() {
    msg.type = ECHO;
    get_timestamp(msg.argument, msg.sender_id);

    mqd_t qid = clients[msg.int_val];
    if (!qid)
        return;

    mq_send(qid, (const char*) &msg, MESSAGE_SIZE, msg.type);
}

void respond_to_list() {
    mqd_t qid = clients[msg.sender_id];
    msg.type = LIST;

    get_online_list();
    get_friends_list();

    mq_send(qid, (const char*) &msg, MESSAGE_SIZE, msg.type);
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
    mqd_t client_queue = clients[msg.sender_id];

    char* token = strtok(msg.argument, " ");
    for (int i = 0; i < MAX_CLIENTS; i++)
        friends[msg.sender_id][i] = -1;

    int i = 0;
    while (token != NULL && i < MAX_CLIENTS) {
        friends[msg.sender_id][i++] = atoi(token);
        token = strtok(NULL, " ");
    }
    mq_send(client_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);
}

void exit_handler() {
    msg.type = STOP;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == 0)
            continue;
        mq_send(clients[i], (const char*) &msg, MESSAGE_SIZE, msg.type);
        mq_close(clients[i]);
    }

    mq_close(server_queue);
    mq_unlink(SERVER_QUEUE_NAME);
    exit(0);
}

int main(int argc, char** argv) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = 0;
        for (int j = 0; j < MAX_CLIENTS; j++)
            friends[i][j] = -1;
    }

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MESSAGE_SIZE;
    attr.mq_curmsgs = 0;

//    mq_unlink(SERVER_QUEUE_NAME);
    if ((server_queue = mq_open(SERVER_QUEUE_NAME, O_RDWR | O_CREAT | O_EXCL, QUEUE_PERMISSIONS, &attr)) == -1)
        exit_errno();

    signal(SIGINT, exit_handler);

    printf("________________________________\n\tChat server start\n________________________________\n");
    printf("Server message queue descriptor: %d\n", server_queue);

    while (1) {
        if (mq_receive(server_queue, (char*) &msg, MESSAGE_SIZE, NULL) == -1) {
            fprintf(stderr, "Error while listening for messages\n");
            continue;
        }
        msg = (struct Message) msg;
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
            case 0:
                break;
            default:
                fprintf(stderr, "Command with id %ld is unrecognised \n", msg.type);
                break;
        }
    }

    return 0;
}
