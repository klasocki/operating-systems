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

void exit_msg(char* msg);

void exit_errno();

void watch_cp(char* filename, int period, int time);

void monitor(char* filename, int period, int time, char* mode, int max_cpu_time, int max_virtual_mem);

void watch_mem(char* filename, int period, int time);

char* read_file(char* filename);

int main(int argc, char* argv[]) {
    if (argc != 6)
        exit_msg("5 arguments required: file_path, monitoring_time (in seconds), mode (cp or mem),"
                 "max_cpu_time, max_virtual_mem");
    char* filepath = argv[1];
    int time = (int) strtol(argv[2], NULL, 10);
    char* mode = argv[3];
    int max_cpu_time = (int) strtol(argv[4], NULL, 10);
    int max_virtual_mem = (int) strtol(argv[5], NULL, 10);
    if (strcmp("cp", mode) && strcmp("mem", mode)) exit_msg("Incorrect monitoring mode (must be cp or mem)");
    FILE* file = fopen(filepath, "r");
    if (!file) exit_errno();
    char* line = malloc(256 * sizeof(char));
    int children_count = 0;
    while (fgets(line, 256, file)) {
        int period;
        char* filename = malloc(256 * sizeof(char));
        if (sscanf(line, "%s %d\n", filename, &period) == 2) {
            pid_t child = fork();
            if (!child) monitor(filename, period, time, mode, max_cpu_time, max_virtual_mem);
            else ++children_count;
        } else {
            fprintf(stderr, "Incorrect format in line \"%s\", ignoring\n", line);
        }
        free(filename);
    }
    free(line);
    for (int i = 0; i < children_count; i++) {
        struct rusage start_usage;
        if (getrusage(RUSAGE_CHILDREN, &start_usage)) exit_errno();
        int status;
        pid_t child_pid = wait(&status);
        if (WIFEXITED(status)) {
            int stat = WEXITSTATUS(status);
            if (stat == 255)
                printf("Process with PID %d was terminated, possibly used too much cpu time or virtual memory,"
                       " behaviour unknown\n", child_pid);
            else printf("Process with PID %d created %d copies\n", child_pid, WEXITSTATUS(status));
        } else {
            printf("Process with PID %d has been terminated, behaviour unknown\n", child_pid);
        }

        struct rusage end_usage;
        if (getrusage(RUSAGE_CHILDREN, &end_usage)) exit_errno();
        printf("Process %d:\nSystem: %ld seconds and %ld nanoseconds\n"
               "User: %ld seconds and %ld nanoseconds\n___________\n",
               child_pid, end_usage.ru_utime.tv_sec - start_usage.ru_utime.tv_sec,
               end_usage.ru_utime.tv_usec - start_usage.ru_utime.tv_usec,
               end_usage.ru_stime.tv_sec - start_usage.ru_stime.tv_sec,
               end_usage.ru_stime.tv_usec - start_usage.ru_stime.tv_usec);
    }
    return 0;
}

void monitor(char* filename, int period, int time, char* mode, int max_cpu_time, int max_virtual_mem) {
    struct rlimit cpu_time_limit, virtual_mem_limit;
    cpu_time_limit.rlim_cur = cpu_time_limit.rlim_max = (rlim_t) max_cpu_time;
    virtual_mem_limit.rlim_cur = virtual_mem_limit.rlim_max = (rlim_t) max_virtual_mem << 20;
    if (setrlimit(RLIMIT_CPU, &cpu_time_limit)) exit_errno();
    if (setrlimit(RLIMIT_AS, &virtual_mem_limit)) exit_errno();
    if (strcmp(mode, "cp") == 0) watch_cp(filename, period, time);
    else if (strcmp(mode, "mem") == 0) watch_mem(filename, period, time);
    else exit_msg("Incorrect monitoring mode");
}

void watch_mem(char* filename, int period, int time) {
    int start = 0;
    int n = 0;
    struct stat stat;
    if (lstat(filename, &stat) == -1) exit_errno();
    char* new_filename = malloc((strlen(filename) + 25) * sizeof(char));
    strcpy(new_filename, filename);
    char* content = read_file(filename);
    time_t last_modified = stat.st_mtime;
    while ((start += period) < time) {
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

void watch_cp(char* filename, int period, int time) {
    int start = 0;
    int n = 0;
    struct stat stat;
    if (lstat(filename, &stat) == -1) exit_errno();
    time_t last_modified = stat.st_mtime;
    char* new_filename = malloc((strlen(filename) + 25) * sizeof(char));
    strcpy(new_filename, filename);
    while ((start += period) < time) {
        if (lstat(filename, &stat) == -1) exit_errno();
        if (last_modified < stat.st_mtime || 0 == n) {
            last_modified = stat.st_mtime;
            pid_t child = vfork();
            if (child == 0) {
                strftime(new_filename + strlen(filename), 25, TIME_FORMAT, gmtime(&last_modified));
                execlp("cp", "cp", filename, new_filename, NULL);
            } else {
                int status;
                wait(&status);
                if (status == 0)
                    n++;
            }
        }
        sleep((unsigned int) period);
    }
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
