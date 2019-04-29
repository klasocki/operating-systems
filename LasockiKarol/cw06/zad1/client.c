#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include "chat.h"

int server_queue_id;
int client_queue_id;
int client_id;
pid_t child_pid;
struct Message msg;

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void execute_commands_from(FILE* stream);

void handle_message();

void send_add(char* command);

void send_del(char* command);

void send_request(int async) {
    msgsnd(server_queue_id, &msg, MESSAGE_SIZE, 0);

    if (!async) {
        struct Message res;
        msgrcv(client_queue_id, &res, MESSAGE_SIZE, 0, 0);

        msg.sender_id = res.sender_id;
        msg.int_val = res.int_val;
        strcpy(msg.argument, res.argument);
    }
}

void send(enum Type type, int async) {
    msg.type = type;
    msg.sender_id = client_id;
    send_request(async);
}

void send_echo(const char* string) {
    strcpy(msg.argument, string);
    send(ECHO, 1);
}

void send_2All(const char* string) {
    strcpy(msg.argument, string);
    send(TO_ALL, 1);
}

void send_2Friends(const char* string) {
    strcpy(msg.argument, string);
    send(TO_FRIENDS, 1);
}

void send_2One(int target_id, const char* string) {
    strcpy(msg.argument, string);
    msg.int_val = target_id;
    send(TO_ONE, 1);
}

void send_list() {
    send(LIST, 1);
}

void send_stop() {
    msg.int_val = client_queue_id;
    send(STOP, 1);
}

void send_friends(const char* string) {
    strcpy(msg.argument, string);
    send(FRIENDS, 1);
}


void send_del(char* string) {
    strcpy(msg.argument, string);
    send(DEL, 1);
}

void send_add(char* string) {
    strcpy(msg.argument, string);
    send(ADD, 1);
}



void send_init() {
    msg.int_val = client_queue_id;
    send(INIT, 0);
    client_id = msg.sender_id;
    printf("Client ID: %d\n", client_id);
}

void client_logout() {
    msgctl(client_queue_id, IPC_RMID, NULL);
    kill(child_pid, SIGKILL);
    exit(0);
}

void exec_command(char* command) {
    if (strncmp(command, "ECHO ", 5) == 0) {
        command += 5;
        send_echo(command);
    } else if (strncmp(command, "2ALL ", 5) == 0) {
        command += 5;
        send_2All(command);
    } else if (strncmp(command, "2FRIENDS ", 9) == 0) {
        command += 9;
        send_2Friends(command);
    } else if (strncmp(command, "ADD ", 4) == 0) {
        command += 4;
        send_add(command);
    } else if (strncmp(command, "DEL ", 4) == 0) {
        command += 4;
        send_del(command);
    }
    else if (strncmp(command, "2ONE ", 5) == 0) {
        command += 5;
        char* token = strtok(command, " ");
        int target = atoi(token);
        token = strtok(NULL, "");
        send_2One(target, token);
    } else if (strncmp(command, "LIST", 4) == 0) {
        send_list();
    } else if (strncmp(command, "STOP", 4) == 0) {
        send_stop();
        client_logout();
    } else if (strncmp(command, "FRIENDS ", 8) == 0) {
        command += 8;
        send_friends(command);
    } else if (strncmp(command, "READ ", 5) == 0) {
        command += 5;

        char* path = calloc(256, sizeof(char));
        strcpy(path, command);
        path[strcspn(path, "\n")] = 0;

        FILE* f = fopen(path, "r");
        if (f == NULL) {
            printf("Unable to open file %s\n", path);
            free(path);
            return;
        }

        kill(child_pid, SIGKILL);
        execute_commands_from(f);
        fclose(f);
        free(path);
    } else if (strcmp(command, "\n") == 0) {} else {
        printf("Invalid command %s\n", command);
    }
}


void execute_commands_from(FILE* stream) {
    char* buffer = calloc(MAX_ARG_LEN, sizeof(char));

    pid_t pid = fork();
    if (pid == 0) {
        while (1) handle_message();
    } else {
        child_pid = pid;
        while (1) {
            if (fgets(buffer, MAX_ARG_LEN, stream) == NULL) break;
            exec_command(buffer);
        }
    }

    free(buffer);
}



void handle_message() {
    if (msgrcv(client_queue_id, &msg, MESSAGE_SIZE, PRIORITY_QUEUE, 0) == -1) {
        return;
    }
    if (msg.type == STOP) client_logout();
    else printf("%s\n", msg.argument);
}

void exit_handler() {
    send_stop();
    client_logout();
}

int main(int argc, char** argv) {
    signal(SIGINT, exit_handler);

    key_t server_key = ftok(SERVER_ID_PATH, SERVER_ID_SEED);

    if ((server_queue_id = msgget(server_key, QUEUE_PERMISSIONS)) == -1) {
        exit_msg("Could not connect to server");
    }

    if ((client_queue_id = msgget(IPC_PRIVATE, QUEUE_PERMISSIONS)) == -1) {
        exit_msg("Could not create private queue");
    }

    printf("Server message queue ID: %d\n", server_queue_id);
    printf("Private message queue ID: %d\n", client_queue_id);

    send_init();
    execute_commands_from(stdin);
}