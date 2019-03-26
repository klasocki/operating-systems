#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <zconf.h>

int WAITING = 0;

void sigtstp_handler(int signum);

void exit_errno() ;

void sigint_handler(int signum);

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP, sigtstp_handler) == SIG_ERR ) exit_errno();
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);

    pid_t pid = fork();
    if (0 == pid) execl("./date-script.sh", "./date-script.sh", NULL);
    int script_terminated = 0;

    while(1) {
        if(!WAITING) {
            if (script_terminated) {
                script_terminated = 0;
                pid = fork();
                if (0 == pid) execl("./date-script.sh", "./date-script.sh", NULL);
            }
        } else{ // WAITING
            if (!script_terminated) {
                kill(pid, SIGKILL);
                script_terminated = 1;
            }
        }
    }
}

void sigtstp_handler(int signum){
    if(!WAITING){
        printf("\nWaiting for CTRL+Z - continue or CTRL+C - exit program\n");
    }
    WAITING = !WAITING;
}

void sigint_handler(int signum){
    printf("\nReceived SIGINT - quiting\n");
    exit(0);
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}

