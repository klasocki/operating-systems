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

sem_t* tape_sem = NULL;
sem_t* load_sem = NULL;
sem_t* truck_sem = NULL;
int employee_pids = -1;
int employee_loads = -1;
int load_times = -1;
int k_fd;
struct timeval* times_arr;
int* loads;
int* K_shared;
pid_t* pids;
pid_t* pids_to_kill;
int* tape_load;
int X;
int K;
int M;
int truck_load = 0;


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
    int sem_val;
    if (sem_getvalue(tape_sem, &sem_val) == -1) exit_errno();
    if (sem_val == 1) take_tape_sem();
    load_remaining();
    printf("Trucker loaded all packages and quiting - killing all the employees...\n");
    for (int i = 0; i < K; i++) {
        if (pids_to_kill[i] != -1) while (kill(pids_to_kill[i], SIGINT) == -1) {if(errno == ESRCH) break;}
    }

    if(munmap((void*) pids_to_kill, 2 * K * sizeof(pid_t)) == -1) exit_errno();
    if(munmap((void*) tape_load, (2 + K) * sizeof(int)) == -1) exit_errno();
    if(munmap((void*) times_arr, K * sizeof(struct timeval)) == -1) exit_errno();
    if(munmap((void*) K_shared, sizeof(int)) == -1) exit_errno();

    if(sem_close(tape_sem) == -1) exit_errno();
    if(sem_close(load_sem) == -1) exit_errno();
    if(sem_close(truck_sem) == -1) exit_errno();

    if(sem_unlink(TAPE_SEM_NAME) == -1) exit_errno();
    if(sem_unlink(TRUCK_SEM_NAME) == -1) exit_errno();
    if(sem_unlink(LOAD_SEM_NAME) == -1) exit_errno();

    if(shm_unlink(SHM_K) == -1) exit_errno();
    if(shm_unlink(SHM_NAME) == -1) exit_errno();
    if(shm_unlink(SHM_NAME1) == -1) exit_errno();
    if(shm_unlink(SHM_NAME2) == -1) exit_errno();
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
    tape_sem = sem_open(TAPE_SEM_NAME, O_CREAT, 0777, 0);
    if (tape_sem == SEM_FAILED) exit_errno();
    load_sem = sem_open(LOAD_SEM_NAME, O_CREAT, 0777, K);
    if (load_sem == SEM_FAILED) exit_errno();
    truck_sem = sem_open(TRUCK_SEM_NAME, O_CREAT, 0777, 0);
    if (truck_sem == SEM_FAILED) exit_errno();

    employee_pids = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0777);
    if (employee_pids == -1) exit_errno();
    ftruncate(employee_pids, 2 * K * sizeof(pid_t));

    pids_to_kill = pids = (pid_t*) mmap(NULL, 2 * K * sizeof(pid_t), PROT_READ | PROT_WRITE,
                                        MAP_SHARED, employee_pids, 0);
    if (pids == MAP_FAILED) exit_errno();

    load_times = shm_open(SHM_NAME1, O_CREAT | O_RDWR, 0777);
    if (load_times == -1) exit_errno();
    ftruncate(load_times, K * sizeof(struct timeval));

    times_arr = (struct timeval*) mmap(NULL, K * sizeof(pid_t), PROT_READ | PROT_WRITE,
                                       MAP_SHARED, load_times, 0);
    if (times_arr == MAP_FAILED) exit_errno();

    employee_loads = shm_open(SHM_NAME2, O_CREAT | O_RDWR, 0777);
    if (employee_loads == -1) exit_errno();
    if(ftruncate(employee_loads, (2 + K) * sizeof(pid_t)) == -1) exit_errno();

    loads = (int*) mmap(NULL, (2 + K) * sizeof(pid_t), PROT_READ | PROT_WRITE,
                        MAP_SHARED, employee_loads, 0);
    if (loads == MAP_FAILED) exit_errno();

    loads[0] = 0; // tape starts with 0 load
    loads[1] = M;
    tape_load = &loads[0];
    loads += 2;

    for (int i = 0; i < K; ++i) {
        loads[i] = 0;
        pids[i] = pids[i + K] = -1;
    }
    pids += K;

    k_fd = shm_open(SHM_K, O_CREAT | O_RDWR, 0777);
    if (k_fd == -1) exit_errno();
    if(ftruncate(k_fd, sizeof(int)) == -1) exit_errno();

    K_shared = (int*) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                        MAP_SHARED, k_fd, 0);
    if (K_shared == MAP_FAILED) exit_errno();
    *K_shared = K;
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
