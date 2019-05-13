#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <zconf.h>
#include <sys/shm.h>

#define TAPE_SEM_NAME "/posixsem"
#define LOAD_SEM_NAME "/posixsem1"
#define TRUCK_SEM_NAME "/posixsem2"
#define SHM_NAME "/posixshm"
#define SHM_NAME1 "/posixshm1"
#define SHM_NAME2 "/posixshm2"
#define SHM_K "/posixshmK"


int C = -1;
int N;
sem_t* tape_sem = NULL;
sem_t* load_sem = NULL;
sem_t* truck_sem = NULL;
int employee_pids = -1;
int employee_loads = -1;
int load_times = -1;
struct timeval* times_arr;
int* loads;
pid_t* pids;
pid_t* pids_to_kill;
int* tape_load;
int K;
int k_fd;
int* K_shared;
int M;


void exit_errno();

void exit_msg(char* msg);

void take_load_sem();

int take_load_nonblock();

void give_load_sem();

void take_truck_sem();

int take_truck_nonblock();

void give_truck_sem();

void give_tape_sem();

void take_tape_sem();

double get_time(struct timeval tm){
    return (double) tm.tv_sec + (double) tm.tv_usec / 1e6;
}

double get_curr_time(){
    struct timeval t;
    gettimeofday(&t, NULL);
    return get_time(t);
}

void init_ipc() {
    tape_sem = sem_open(TAPE_SEM_NAME, 0);
    if (tape_sem == SEM_FAILED) exit_msg("Could not access semaphore, make sure trucker is started first");
    load_sem = sem_open(LOAD_SEM_NAME, 0);
    if (load_sem == SEM_FAILED) exit_msg("Could not access semaphore, make sure trucker is started first");
    truck_sem = sem_open(TRUCK_SEM_NAME, 0);
    if (truck_sem == SEM_FAILED) exit_msg("Could not access semaphore, make sure trucker is started first");

    k_fd = shm_open(SHM_K, O_RDWR, 0777);
    if (k_fd == -1) exit_errno();
    if(ftruncate(k_fd, sizeof(int)) == -1) exit_errno();

    K_shared = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                           MAP_SHARED, k_fd, 0);
    if (K_shared == MAP_FAILED) exit_errno();
    K = *K_shared;

    employee_loads = shm_open(SHM_NAME2, O_RDWR, 0777);
    if (employee_loads == -1) exit_msg("Could not access shared mem, make sure trucker is started first");

    ftruncate(employee_loads, (2 + K) * sizeof(int));
    loads = (int*) mmap(NULL, (2 + K) * sizeof(int), PROT_READ | PROT_WRITE,
                        MAP_SHARED, employee_loads, 0);
    if (loads == MAP_FAILED) exit_errno();
    M = loads[1];
    tape_load = &loads[0];
    loads += 2;

    employee_pids = shm_open(SHM_NAME, O_RDWR, 0777);
    if (employee_pids == -1) exit_msg("Could not access shared mem, make sure trucker is started first");
    ftruncate(employee_pids, 2 * K * sizeof(pid_t));

    pids_to_kill = pids = (pid_t*) mmap(NULL, 2 * K * sizeof(pid_t), PROT_READ | PROT_WRITE | PROT_EXEC,
                                        MAP_SHARED, employee_pids, 0);
    if (pids == MAP_FAILED) exit_errno();

    load_times = shm_open(SHM_NAME1, O_RDWR, 0777);
    if (load_times == -1) exit_msg("Could not access shared mem, make sure trucker is started first");
    ftruncate(load_times, K * sizeof(struct timeval));

    times_arr = (struct timeval*) mmap(NULL, K * sizeof(pid_t), PROT_READ | PROT_WRITE,
                                       MAP_SHARED, load_times, 0);
    if (times_arr == MAP_FAILED) exit_errno();

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
    int loaded = 0;
    int try_number = -1;
    while(!loaded) {
        if(try_number < 1)  ++try_number;
        if(try_number == 0){
            if (take_load_nonblock() == -1) {
                printf("No space on tape, employee %d waiting for truck to load...[%.6f]\n", getpid(), get_curr_time());
                take_load_sem();
            }
        } else{
            take_load_sem();
        }
        take_tape_sem();
        int i;
        for (i = 0; i < K && loads[i] != 0; i++) {} // find free spot on tape to place package
        if (i != K && *tape_load + N <= M) {
            printf("Loader %d loading %d [%.6f]\n", getpid(), N, get_curr_time());
            loads[i] = N;
            pids[i] = getpid();
            times_arr[i] = current_time;
            *tape_load += N;
            loaded = 1;
            give_truck_sem();
        } else {
            if(try_number == 0) printf("Tape overweighted, employee %d waiting for truck to load...[%.6f]\n", getpid(), get_curr_time());
            give_load_sem();
        }
        give_tape_sem();
    }
}

void exit_fun() {
    if(munmap((void*) pids_to_kill, 2 * K * sizeof(pid_t)) == -1) exit_errno();
    if(munmap((void*) tape_load, (2 + K) * sizeof(int)) == -1) exit_errno();
    if(munmap((void*) times_arr, K * sizeof(struct timeval)) == -1) exit_errno();
    if(munmap((void*) K_shared, sizeof(int)) == -1) exit_errno();
    if(sem_close(tape_sem) == -1) exit_errno();
    if(sem_close(load_sem) == -1) exit_errno();
    if(sem_close(truck_sem) == -1) exit_errno();}

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

void take_tape_sem() {
    if (sem_wait(tape_sem) == -1) exit_errno();
}

void give_tape_sem() {
    if (sem_post(tape_sem) == -1) exit_errno();
}

void take_load_sem() {
    if (sem_wait(load_sem) == -1) exit_errno();
}

void give_load_sem() {
    if (sem_post(load_sem) == -1) exit_errno();
}

void take_truck_sem() {
    if (sem_wait(truck_sem) == -1) exit_errno();
}

void give_truck_sem() {
    if (sem_post(truck_sem) == -1) exit_errno();
}

int take_truck_nonblock(){
    int res = sem_trywait(truck_sem);
    if(res == -1 && errno != EAGAIN) exit_errno();
    return res;
}

int take_load_nonblock(){
    int res = sem_trywait(load_sem);
    if(res == -1 && errno != EAGAIN) exit_errno();
    return res;
}
