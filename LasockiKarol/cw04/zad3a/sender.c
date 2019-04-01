#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include <zconf.h>


void exit_errno();

void exit_msg(char* msg);

void send_signals(int catcher_pid, int signal, int end_signal);

int signal_count;
int received_count = 0;

void add_handler(void(* fun)(int, siginfo_t*, void*), int signal, int signal2) {
    sigset_t signals;
    if (sigfillset(&signals) == -1) exit_errno();
    sigdelset(&signals, signal);
    sigdelset(&signals, signal2);
    if (sigprocmask(SIG_BLOCK, &signals, NULL)) exit_errno();
    if (sigemptyset(&signals)) exit_errno();

    struct sigaction action;
    action.sa_sigaction = fun;
    action.sa_mask = signals;
    action.sa_flags = SA_SIGINFO;

    if (sigaction(signal, &action, NULL) == -1 || sigaction(signal2, &action, NULL) == -1) exit_errno();
}

void kill_handler(int signum, siginfo_t* siginfo, void* _) {
    if (signum == SIGUSR1) ++received_count;
    else {
        printf("Should reveive %d signals, received %d\n", signal_count, received_count);
        exit(0);
    }
}

void queue_handler(int signum, siginfo_t* siginfo, void* _) {
    if (signum == SIGUSR1) {
        ++received_count;
        printf("Signal number %d caught\n", siginfo->si_value.sival_int);
    } else {
        printf("Should reveive %d signals, received %d\n", signal_count, received_count);
        exit(0);
    }
}

void realtime_handler(int signum, siginfo_t* siginfo, void* _) {
    if (signum == SIGRTMIN) ++received_count;
    else {
        printf("Should reveive %d signals, received %d\n", signal_count, received_count);
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4)
        exit_msg("Required 3 arguments: catcher process PID, num of signals, mode (KILL, SIGQUEUE"
                 " or SIGRT)");
    int catcher_pid = strtol(argv[1], NULL, 10);
    signal_count = strtol(argv[2], NULL, 10);
    char* mode = argv[3];

    if (strcmp(mode, "KILL") == 0) {
        add_handler(kill_handler, SIGUSR1, SIGUSR2);
        send_signals(catcher_pid, SIGUSR1, SIGUSR2);
    } else if (strcmp(mode, "SIGQUEUE") == 0) {
        add_handler(queue_handler, SIGUSR1, SIGUSR2);
        for (int i = 0; i < signal_count; i++) {
            union sigval val;
            val.sival_int = i;
            if (sigqueue(catcher_pid, SIGUSR1, val)) exit_errno();
        }
        if (kill(catcher_pid, SIGUSR2)) exit_errno();
    } else if (strcmp(mode, "SIGRT") == 0) {
        add_handler(realtime_handler, SIGRTMIN, SIGRTMAX);
        send_signals(catcher_pid, SIGRTMIN, SIGRTMAX);
    } else
        exit_msg("Invalid mode, expected (KILL, SIGQUEUE or SIGRT)");

    while(1) pause();
    return 0;
}

void send_signals(int catcher_pid, int signal, int end_signal) {
    for (int i = 0; i < signal_count; i++)
        if (kill(catcher_pid, signal)) exit_errno();
    if (kill(catcher_pid, end_signal)) exit_errno();
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}