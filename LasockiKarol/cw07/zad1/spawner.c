#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zconf.h>
#include <time.h>

void exit_errno();

void exit_msg(char* msg);

int main(int argc, char* argv[]) {
    if (argc < 3)
        exit_msg("Arguments required: number of loaders, loader max load; optional: loader life-cycle length");
    int N = (int) strtol(argv[1], NULL, 10);
    int max = (int) strtol(argv[2], NULL, 10);
    srand(time(NULL));
    for (int i = 0; i < N; i++) {
        int load = rand() % max + 1;
        char buff[10];
        sprintf(buff, "%d", load);
        if (fork() == 0) {
            if (argc == 4) {
                if(execlp("./loader", "./loader", buff, argv[3], NULL) == -1) exit_errno();
            } else
                if(execlp("./loader", "./loader", buff, NULL) == -1) exit_errno();
        }
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