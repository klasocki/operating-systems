#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <time.h>
#include <zconf.h>

#define TIME_FORMAT "%Y-%m-%d_%H-%M-%S"

void exit_msg(char* msg);

void exit_errno();

int main(int argc, char* argv[]) {
    if (argc != 6) exit_msg("Required 5 arguments: filename, pmin, pmax, total_time, bytes");
    char* filename = argv[1];
    int pmin = (int) strtol(argv[2], NULL, 10);
    int pmax = (int) strtol(argv[3], NULL, 10);
    int total_time = (int) strtol(argv[4], NULL, 10);
    int bytes = (int) strtol(argv[5], NULL, 10);
    srand((unsigned int) time(NULL));
    char* date = malloc(25 * sizeof(char));
    char* line = malloc((bytes + 1) * sizeof(char));
    for (int i = 0; i < bytes; i++) line[i] = 'x';
    line[bytes] = '\0';
    int start = 0;
    while (start < total_time) {
        int wait = rand() % (abs(pmax - pmin) + 1) + pmin;
        start += wait;
        sleep((unsigned int) wait);
        FILE* file = fopen(filename, "a");
        if (!file) exit_errno();
        time_t t = time(NULL);
        strftime(date, 25, TIME_FORMAT, gmtime(&t));
        fprintf(file, "%d %s %s\n", wait, date, line);
        fclose(file);
    }
    return 0;
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}

