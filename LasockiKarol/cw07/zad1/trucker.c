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
int tape_sem = -1;
int employee_pids = -1;
int employee_loads = -1;
int load_times = -1;
struct timeval* times_arr;
int* loads;
pid_t* pids;
pid_t* pids_to_kill;
int* tape_load;
int X;
int K;
int M;
int truck_load = 0;

union semun {
    int val;    /* Value for SETVAL */
    struct semid_ds* buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short* array;  /* Array for GETALL, SETALL */
    struct seminfo* __buf;  /* Buffer for IPC_INFO
                                           (Linux-specific) */
};

void take_load_sem();

int take_load_nonblock();

void give_load_sem();

void take_truck_sem();

int take_truck_nonblock();

void give_truck_sem();

void give_tape_sem();

void take_tape_sem();

void exit_errno();

void exit_msg(char* msg);

void load_remaining();

void load_package_to_truck();

double get_time(struct timeval tm) {
    return (double) tm.tv_sec + (double) tm.tv_usec / 1e6;
}

double get_curr_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return get_time(t);
}

double time_diff(struct timeval start) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return get_time(t) - get_time(start);
}

void sigint_handle(int signum) {
    exit(0);
}

void exit_fun() {
    printf("\nRunning out of trucks! Loading remaining packages...\n\n");
    int sem_val = semctl(tape_sem, 0, GETVAL);
    if (sem_val == -1) exit_errno();
    if (sem_val == 1) take_tape_sem();
    load_remaining();
    printf("Trucker loaded all packages and quiting - killing all the employees...\n");
    for (int i = 0; i < K; i++) {
        if (pids_to_kill[i] != -1) while (kill(pids_to_kill[i], SIGINT) == -1) { if (errno == ESRCH) break; }
    }
    give_tape_sem();
    shmdt((void*) pids_to_kill);
    shmdt((void*) loads);
    shmdt((void*) times_arr);
    semctl(tape_sem, 0, IPC_RMID, 0);
    shmctl(employee_loads, IPC_RMID, NULL);
    shmctl(employee_pids, IPC_RMID, NULL);
    shmctl(load_times, IPC_RMID, NULL);
    exit(0);
}

void load_remaining() {
    while (loads[0] != 0) {
        if (truck_load + loads[0] <= X) {
            load_package_to_truck();
        } else {
            printf("Loaded truck is leaving with load %d...[%.6f]\n", truck_load, get_curr_time());
            truck_load = 0;
            printf("Empty truck arriving... [%.6f]\n", get_curr_time());
        }
    }
}

void load_package_to_truck() {
    truck_load += loads[0];
    *tape_load -= loads[0];
    printf("Employee %d loaded %d weight to the truck, space left: %d, space taken: %d, time taken: %.6f seconds\n",
           pids[0], loads[0], X - truck_load, truck_load, time_diff(times_arr[0]));
    // move packages
    for (int i = 1; i < K; ++i) {
        loads[i - 1] = loads[i];
        pids[i - 1] = pids[i];
        times_arr[i - 1] = times_arr[i];
    }
    loads[K - 1] = 0;
    pids[K - 1] = -1;
}

void init_ipc() {
    tape_sem = semget(SEM_KEY, 3, 0777 | IPC_CREAT);
    if (tape_sem == -1) exit_errno();

    union semun arg;
    arg.val = 0;
    if (semctl(tape_sem, 0, SETVAL, arg) == -1) exit_errno();
    arg.val = K;
    if (semctl(tape_sem, 1, SETVAL, arg) == -1) exit_errno();
    arg.val = 0;
    if (semctl(tape_sem, 2, SETVAL, arg) == -1) exit_errno();

    employee_pids = shmget(SHM_KEY, 2 * K * sizeof(pid_t), 0777 | IPC_CREAT);
    if (employee_pids == -1) exit_errno();

    pids_to_kill = pids = (pid_t*) shmat(employee_pids, NULL, 0);
    if (pids == (void*) -1) exit_errno();

    load_times = shmget(SHM_KEY + 1, K * sizeof(struct timeval), 0777 | IPC_CREAT);
    if (load_times == -1) exit_errno();

    times_arr = (struct timeval*) shmat(load_times, NULL, 0);
    if (times_arr == (void*) -1) exit_errno();

    employee_loads = shmget(SHM_KEY + 2, (K + 3) * sizeof(int), 0777 | IPC_CREAT);
    if (employee_loads == -1) exit_errno();

    loads = shmat(employee_loads, NULL, 0);
    if (loads == (void*) -1) exit_errno();

    loads = (int*) loads;
    loads[0] = 0; // tape starts with 0 load
    loads[1] = K;
    loads[2] = M;
    tape_load = &loads[0];
    loads += 3;

    for (int i = 0; i < K; ++i) {
        loads[i] = 0;
        pids[i] = pids[i + K] = -1;
    }
    pids += K;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        exit_msg("Required 3 arguments, x - truck max load, k - tape max number, m - tape max load");
    }
    X = (int) strtol(argv[1], NULL, 10);
    K = (int) strtol(argv[2], NULL, 10);
    M = (int) strtol(argv[3], NULL, 10);
    atexit(exit_fun);
    struct sigaction sa;
    sa.sa_handler = sigint_handle;
    sigaction(SIGINT, &sa, NULL);

    init_ipc();

    while (1) {
        truck_load = 0;
        printf("Empty truck arriving... [%.6f]\n", get_curr_time());
        give_tape_sem();
        while (1) {
            if (take_truck_nonblock() == -1) {
                printf("Truck waiting for packages...[%.6f]\n", get_curr_time());
                take_truck_sem();
            }
            take_tape_sem();
            if (truck_load + loads[0] <= X) {
                load_package_to_truck();
                give_tape_sem();
                give_load_sem();
            } else {
                printf("Loaded truck is leaving with load %d...[%.6f]\n", truck_load, get_curr_time());
                give_truck_sem();
                break;
            }
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

int operate(int val, int sem, int sem_flg) {
    struct sembuf sem_action;
    sem_action.sem_flg = sem_flg;
    sem_action.sem_num = sem;
    sem_action.sem_op = val;
    int res = semop(tape_sem, &sem_action, 1);
    if (res == -1 && errno != EAGAIN) exit_msg("");
    return res;
}

void take_tape_sem() {
    operate(-1, 0, 0);
}

void give_tape_sem() {
    operate(1, 0, 0);
}

void take_load_sem() {
    operate(-1, 1, 0);
}

void give_load_sem() {
    operate(1, 1, 0);
}

void take_truck_sem() {
    operate(-1, 2, 0);
}

void give_truck_sem() {
    operate(1, 2, 0);
}

int take_load_nonblock() {
    return operate(-1, 1, IPC_NOWAIT);
}

int take_truck_nonblock() {
    return operate(-1, 2, IPC_NOWAIT);
}


