#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <zconf.h>


void exit_errno();

void exit_msg(char* msg);

int received_count = 0;

void add_handler(void(* fun)(int, siginfo_t*, void*), int signal, int signal2) {
    sigset_t signals;
    if (sigfillset(&signals) == -1) exit_errno();
    sigdelset(&signals, signal);
    sigdelset(&signals, signal2);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) exit_errno();
    if (sigemptyset(&signals)) exit_errno();

    struct sigaction action;
    action.sa_mask = signals;
    action.sa_sigaction = fun;
    action.sa_flags = SA_SIGINFO;

    if (sigaction(signal, &action, NULL) == -1 || sigaction(signal2, &action, NULL) == -1) exit_errno();
}

void kill_handler(int signum, siginfo_t* siginfo, void* _) {
    if (signum == SIGUSR1) {
        ++received_count;
        if(kill(siginfo->si_pid, SIGUSR1)) exit_errno();
    }
    else {
        if(kill(siginfo->si_pid, SIGUSR2)) exit_errno();
        exit(0);
    }
}


void queue_handler(int signum, siginfo_t* siginfo, void* _) {
    pid_t sender_pid = siginfo->si_pid;
    if (signum == SIGUSR1) {
        ++received_count;
        if(sigqueue(sender_pid, SIGUSR1, siginfo->si_value)) exit_errno();
    }
    else {
        if(kill(sender_pid, SIGUSR2)) exit_errno();
        exit(0);
    }
}

void realtime_handler(int signum, siginfo_t* siginfo, void* _) {
    if (signum == SIGRTMIN) {
        ++received_count;
        if(kill(siginfo->si_pid, SIGRTMIN)) exit_errno();
    }
    else {
        if(kill(siginfo->si_pid, SIGRTMAX)) exit_errno();
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2)
        exit_msg("Required mode (KILL, SIGQUEUE or SIGRT)");
    char* mode = argv[1];
    if (strcmp(mode, "KILL") == 0) {
        add_handler(kill_handler, SIGUSR1, SIGUSR2);
    } else if (strcmp(mode, "SIGQUEUE") == 0) {
        add_handler(queue_handler, SIGUSR1, SIGUSR2);
    } else if (strcmp(mode, "SIGRT") == 0) {
        add_handler(realtime_handler, SIGRTMIN, SIGRTMAX);
    } else
        exit_msg("Invalid mode, expected (KILL, SIGQUEUE or SIGRT)");
    printf("Catcher PID: %d\n", (int) getpid());

    while(1) pause();

    return 0;
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}