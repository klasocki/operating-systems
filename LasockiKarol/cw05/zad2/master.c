#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zconf.h>

#define MAX_LINE_LEN 64

void exit_errno();

void exit_msg(char* msg);

int main(int argc, char* argv[]) {
    if(argc!=2) exit_msg("Path to named pipeline required");
    char buffer[MAX_LINE_LEN];
    if(mkfifo(argv[1], S_IRUSR | S_IWUSR)) exit_errno();
    FILE* pipeline = fopen(argv[1], "r");
    if(!pipeline) exit_errno();

    while (fgets(buffer, MAX_LINE_LEN, pipeline)) {
        printf("%s", buffer);
    }
    if(fclose(pipeline) == EOF) exit_errno();
    return 0;
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}