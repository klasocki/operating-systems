#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <stdlib.h>
#include <zconf.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

#define TIME_FORMAT "_%Y-%m-%d_%H-%M-%S"

struct process {
    pid_t pid;
    char* filename;
    int period;
    int stopped;
};

void exit_msg(char* msg);

void exit_errno();

pid_t monitor(char* filename, int period);

void watch_mem(char* filename, int period);

char* read_file(char* filename);

int count_lines(char* filename);

void list(struct process* processes, int count);

void start(struct process* process);

void stop(struct process* process);

struct process* find_process(pid_t pid, struct process* processes, int count);

int EXITED = 0;
int STOPPED = 0;


void sigint_handler(int signum) {
    if (signum == SIGINT) EXITED = 1;
}

int main(int argc, char* argv[]) {
    struct sigaction handler;
    handler.sa_handler = sigint_handler;
    sigaction(SIGINT, &handler, NULL);

    if (argc != 2)
        exit_msg("Required 1 argument - path to file with list of files to monitor\n");
    char* filepath = argv[1];
    FILE* file = fopen(filepath, "r");
    if (!file) exit_errno();
    char* line = malloc(256 * sizeof(char));
    int children_count = 0;

    int n_files = count_lines(filepath);
    struct process* processes = malloc(n_files * sizeof(struct process));

    while (fgets(line, 256, file)) {
        int period;
        char* filename = malloc(256 * sizeof(char));
        if (sscanf(line, "%s %d\n", filename, &period) == 2) {
            pid_t child = monitor(filename, period);
            if (child) {
                struct process* process = &processes[children_count];
                process->pid = child;
                process->filename = malloc((strlen(filename) + 1) * sizeof(char));
                strcpy(process->filename, filename);
                process->period = period;
                process->stopped = 0;
                ++children_count;
            }
        } else {
            fprintf(stderr, "Incorrect format in line \"%s\", ignoring\n", line);
        }
        free(filename);
    }
    free(line);

    char command[20];


    while (!EXITED) {
        fgets(command, 20, stdin);
        if (strcmp(command, "LIST\n") == 0) {
            list(processes, children_count);
        } else if (strcmp(command, "STOP ALL\n") == 0) {
            for (int i = 0; i < children_count; i++) {
                stop(&processes[i]);
            }
        } else if (strcmp(command, "START ALL\n") == 0) {
            for (int i = 0; i < children_count; i++) {
                start(&processes[i]);
            }
        } else if (strncmp(command, "STOP ", 5) == 0) {
            int pid = strtol(command + 5, NULL, 10);
            struct process* process = find_process(pid, processes, children_count);
            if (process) stop(process);
            else fprintf(stderr, "Could not find process with PID %d", pid);
        } else if (strncmp(command, "START ", 6) == 0) {
            int pid = strtol(command + 6, NULL, 10);
            struct process* process = find_process(pid, processes, children_count);
            if (process) start(process);
            else fprintf(stderr, "Could not find process with PID %d\n", pid);
        } else if (strcmp(command, "END\n") == 0 || EXITED) {
            break;
        } else {
            fprintf(stderr, "Invalid command %s\n", command);
        }
    }

    for (int i = 0; i < children_count; i++) {
        int status;
        kill(processes[i].pid, SIGUSR2);
        pid_t child_pid = waitpid(processes[i].pid, &status, 0);
        if (WIFEXITED(status)) {
            int stat = WEXITSTATUS(status);
            if (stat == 255)
                printf("Process with PID %d was terminated due to an error"
                       " behaviour unknown\n", child_pid);
            else printf("Process with PID %d created %d copies\n", child_pid, WEXITSTATUS(status));
        } else {
            printf("Process with PID %d has been terminated, behaviour unknown\n", child_pid);
        }
    }

    for (int i = 0; i < children_count; i++) {
        free(processes[i].filename);
    }
    free(processes);
    return 0;
}

struct process* find_process(pid_t pid, struct process* processes, int count) {
    for (int i = 0; i < count; i++) {
        if (processes[i].pid == pid) return &processes[i];
    }
    return NULL;
}

void start(struct process* process) {
    if (!process) return;
    if (!process->stopped) {
        printf("Processs %d was already running\n", process->pid);
    } else {
        kill(process->pid, SIGUSR1);
        process->stopped = 0;
        printf("Process %d started\n", process->pid);
    }
}

void list(struct process* processes, int count) {
    for (int i = 0; i < count; i++) {
        printf("Process %d is monitoring file %s in %d second periods\n",
               processes[i].pid, processes[i].filename, processes[i].period);
    }
}

void stop(struct process* process) {
    if (!process) return;
    if (process->stopped) {
        printf("Process %d was already stopped\n", process->pid);
    } else {
        kill(process->pid, SIGUSR1);
        process->stopped = 1;
        printf("Process %d stopped\n", process->pid);
    }
}

void monitor_handler(int signum) {
    if (signum == SIGUSR1) {
        STOPPED = !STOPPED;
    } else {
        EXITED = 1;
    }
}

pid_t monitor(char* filename, int period) {
    pid_t child = fork();
    if (!child) {
        struct sigaction handler;
        handler.sa_handler = monitor_handler;
        sigaction(SIGUSR1, &handler, NULL);
        sigaction(SIGUSR2, &handler, NULL);
        watch_mem(filename, period);
    }
    return child;
}

void watch_mem(char* filename, int period) {
    int n = 0;
    struct stat stat;
    if (lstat(filename, &stat) == -1) exit_errno();
    char* new_filename = malloc((strlen(filename) + 25) * sizeof(char));
    strcpy(new_filename, filename);
    char* content = read_file(filename);
    time_t last_modified = stat.st_mtime;
    while (1) {
        if (EXITED) break;
        if (STOPPED) continue;
        if (lstat(filename, &stat) == -1) exit_errno();
        if (last_modified < stat.st_mtime) {
            last_modified = stat.st_mtime;
            strftime(new_filename + strlen(filename), 25, TIME_FORMAT, gmtime(&last_modified));
            FILE* copy = fopen(new_filename, "w");
            if (!copy) exit_errno();
            fwrite(content, sizeof(char), strlen(content), copy);
            if (fclose(copy)) exit_errno();
            content = read_file(filename);
            if (!content) exit_errno();
            n++;
        }
        sleep((unsigned int) period);
    }
    free(content);
    free(new_filename);
    exit(n);
}

char* read_file(char* filename) {
    struct stat stat;
    if (lstat(filename, &stat) != 0) exit_errno();
    FILE* file = fopen(filename, "r");
    if (!file) exit_errno();
    char* buffer = malloc((stat.st_size + 1) * sizeof(char));
    if (fread(buffer, sizeof(char), (size_t) stat.st_size, file) != stat.st_size)
        exit_msg("Error while reading file");
    buffer[stat.st_size] = '\0';
    if (fclose(file)) exit_errno();
    return buffer;
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(255);
}

void exit_errno() {
    exit_msg(strerror(errno));
}

int count_lines(char* filename) {
    char* content = read_file(filename);
    int count = 0;
    while (content) {
        content = strchr(content, '\n');
        if (content) {
            content++;
            count++;
        }
    }
    return count;
}
