#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <unistd.h>
#include <mqueue.h>
#include "chat.h"

mqd_t server_queue;
mqd_t client_queue;
int client_id;
pid_t child_pid;
pid_t parrent_pid;
struct Message msg;
char* queue_name();

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void execute_commands_from(FILE* stream);

void handle_message();

void send_add(char* string);

void send_del(char* string);

void send_request(int async) {
    mq_send(server_queue, (const char*) &msg, MESSAGE_SIZE, msg.type);

    if (!async) {
        mq_receive(client_queue, (char*) &msg, MESSAGE_SIZE, NULL);
        msg = (struct Message) msg;
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
    msg.int_val = client_queue;
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
    memset(msg.argument, 0, sizeof(msg.argument));
    strcat(msg.argument, queue_name());
    send(INIT, 0);
    client_id = msg.sender_id;
    printf("Client ID: %d\n", client_id);
    parrent_pid = getpid();
}

void client_logout() {
    mq_close(client_queue);
    mq_close(server_queue);
    mq_unlink(queue_name());
    kill(parrent_pid, SIGKILL);
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
    if (mq_receive(client_queue, (char*) &msg, MESSAGE_SIZE, NULL) == -1) {
        return;
    }
    msg = (struct Message) msg;
    if (msg.type == STOP) client_logout();
    else printf("%s\n", msg.argument);
}

void exit_handler() {
    send_stop();
    client_logout();
}

char* queue_name() {
    char *name = calloc(32, sizeof(char));
    name[0] = '/';
    sprintf(name + 1, "%d", getpid());
    return name;
}

int main(int argc, char** argv) {
    signal(SIGINT, exit_handler);

    if((server_queue = mq_open(SERVER_QUEUE_NAME, O_WRONLY)) < 0) exit_msg("Could not reach server");

    struct mq_attr attr;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MESSAGE_SIZE;
    attr.mq_flags = 0;
    attr.mq_curmsgs = 0;

//    mq_unlink(queue_name());
    if((client_queue = mq_open(queue_name(), O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr)) < 0)
        exit_msg("Trouble logging to chat");

    printf("Server message queue ID: %d\n", server_queue);
    printf("Private message queue ID: %d\n", client_queue);

    send_init();
    execute_commands_from(stdin);
}