#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <semaphore.h>
#include <sys/times.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <zconf.h>

#define SEM_KEY 147
#define SHM_KEY 17
int N;
int C = -1;
int semaphore = -1;
int employee_pids = -1;
int employee_loads = -1;
int load_times = -1;
struct timeval* times_arr;
int* loads;
pid_t* pids;
pid_t* pids_to_kill;
int* tape_load;
int K;
int M;


void exit_errno();

void exit_msg(char* msg);

void give_sem();

void take_sem();

double get_time(struct timeval tm){
    return (double) tm.tv_sec + (double) tm.tv_usec / 1e6;
}

double get_curr_time(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return get_time(t);
}

void init_ipc() {
    semaphore = semget(SEM_KEY, 0, 0777);
    if (semaphore == -1) exit_msg("Could not access semaphore, make sure trucker is started first");

    employee_pids = shmget(SHM_KEY, 0, 0777);
    if (employee_pids == -1) exit_msg("Could not access shared mem, make sure trucker is started first");

    pids_to_kill = pids = (pid_t*) shmat(employee_pids, NULL, 0);
    if (pids == (void*) -1) exit_errno();

    load_times = shmget(SHM_KEY + 1, 0, 0777);
    if (load_times == -1) exit_msg("Could not access shared mem, make sure trucker is started first");

    times_arr = (struct timeval*) shmat(load_times, NULL, 0);
    if (times_arr == (void*) -1) exit_errno();

    employee_loads = shmget(SHM_KEY + 2, 0, 0777);
    if (employee_loads == -1) exit_msg("Could not access shared mem, make sure trucker is started first");

    loads = shmat(employee_loads, NULL, 0);
    if (loads == (void*) -1) exit_errno();

    loads = (int*) loads;
    K = loads[1];
    M = loads[2];
    tape_load = &loads[0];
    loads += 3;

    pids += K;
    // sign up at the trucker kill list
    int i;
    for(i = 0; i < K && pids_to_kill[i] != -1; i++) {}
    if( i < K) pids_to_kill[i] = getpid();
    else exit_msg("Too many loaders!");
}

void load() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    take_sem(0);
    int i;
    for (i = 0; i < K && loads[i] != 0; i++) {} // find free spot on tape to place package
    if (*tape_load + N < M && i < K) {
        *tape_load += N;
        printf("Loader %d loading %d [%.6f]\n", getpid(), N, get_curr_time());
        loads[i] = N;
        pids[i] = getpid();
        times_arr[i] = current_time;
        give_sem(0);
    } else {
        printf("No space on tape, employee %d waiting for truck to load...[%.6f]\n", getpid(), get_curr_time());
        give_sem(0);
        sleep(1);
    }
}

void exit_fun() {
    shmdt((void*) pids);
    shmdt((void*) loads);
    shmdt((void*) times_arr);
}

void sigint_handle(int signum){
    printf("Employee %d killed by trucker\n", getpid());
    exit(0);
}

int main(int argc, char* argv[]) {
    sleep(1); // to make sure trucker is created when running in parallel from make
    if (argc < 2) {
        exit_msg("Required 1 argument: N - package weight, optional: C - number of cycles");
    }
    atexit(exit_fun);
    struct sigaction sa;
    sa.sa_handler = sigint_handle;
    sigaction(SIGINT, &sa, NULL);
    init_ipc();
    N = (int) strtol(argv[1], NULL, 10);
    if (argc == 3) C = (int) strtol(argv[2], NULL, 10);
    if (C == -1) {
        while (1) {
            load();
        }
    } else {
        while (C--) {
            load();
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

void operate(int val) {
    struct sembuf sem_action;
    sem_action.sem_flg = SEM_UNDO;
    sem_action.sem_num = 0;
    sem_action.sem_op = val;
    semop(semaphore, &sem_action, 1);
}

void take_sem() {
    operate(-1);
}

void give_sem() {
    operate(1);
}

