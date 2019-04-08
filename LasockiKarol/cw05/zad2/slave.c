#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zconf.h>
#include <fcntl.h>

#define MAX_LINE_LEN 64

void exit_errno();

void exit_msg(char* msg);

int main(int argc, char* argv[]) {
    sleep(1); // so that fifo is always created when running master and slave in parallel
    if(argc!=3) exit_msg("Path to named pipeline and N (times date will be written) required");
    int n = strtol(argv[2], NULL, 10);
    srand(time(NULL));
    char date[MAX_LINE_LEN - 16];
    char string[MAX_LINE_LEN];
    printf("Slave PID: %d\n", (int) getpid());
    int pipeline = open(argv[1], O_WRONLY);
    if(pipeline == -1) exit_errno();
    for (int i = 0; i < n; i++) {
        FILE* date_pipe = popen("date", "r");
        if(!date_pipe) exit_errno();
        fgets(date, MAX_LINE_LEN - 16, date_pipe);
        snprintf(string, MAX_LINE_LEN, "%d - %s", (int) getpid(), date);
        write(pipeline, string, strlen(string));
        if(fclose(date_pipe) == EOF) exit_errno();
        sleep(rand() % 4 + 2);
    }
    if(close(pipeline) == -1) exit_errno();
    return 0;
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}