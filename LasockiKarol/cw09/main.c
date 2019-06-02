#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <pthread.h>

#define exit_msg(format, ...) { log(format, ##__VA_ARGS__); exit(1);}
#define log(format, ...) {printf("\e[1;36m[%.6f]:\e[0m ",get_curr_time()); printf(format, ##__VA_ARGS__); printf("\n");}

double get_time(struct timeval tm) {
    return (double) tm.tv_sec + (double) tm.tv_usec / 1e6;
}

double get_curr_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return get_time(t);
}

typedef struct Car {
    int id;
    int current_people_count;
    int status;
    int run_number;
    pthread_mutex_t access;
    pthread_cond_t car_change;
    pthread_cond_t passenger_change;
} Car;

#define AT_STOP 3
#define ACCEPTING_PASSENGERS 0
#define WAITING_FOR_START 1
#define RIDING 2

void cleanup(void);

void remove_car(Car* car);

Car* new_car(int order_number);

void* run_car(void* args);

void* run_passenger(void* args);

int passenger_count, car_count, car_capacity, run_count, cars_available;
int car_to_run_next = 0;
int car_to_finish_next = 0;
Car* current_car = NULL;
Car** cars;
pthread_t** car_threads;
pthread_t** passenger_threads;
int* passenger_ids;
pthread_cond_t* car_to_run_next_condition;
pthread_cond_t* car_availible_condition;
pthread_cond_t* car_to_finish_next_cond;
pthread_mutex_t* car_access_mutex;
pthread_mutex_t* car_stop_mutex;
pthread_mutex_t* car_finish_mutex;

int main(int argc, char** argv) {
    int i;
    if (argc != 5) exit_msg("Required 4 arguments: Passenger count, car count, car capacity and number of total runs")
    passenger_count = strtol(argv[1], NULL, 10);
    car_count = strtol(argv[2], NULL, 10);
    car_capacity = strtol(argv[3], NULL, 10);
    run_count = strtol(argv[4], NULL, 10);
    cars_available = car_count;

    car_to_run_next_condition = malloc(sizeof(pthread_cond_t));
    car_availible_condition = malloc(sizeof(pthread_cond_t));
    car_to_finish_next_cond = malloc(sizeof(pthread_cond_t));
    car_access_mutex = malloc(sizeof(pthread_mutex_t));
    car_stop_mutex = malloc(sizeof(pthread_mutex_t));
    car_finish_mutex = malloc(sizeof(pthread_mutex_t));

    if (pthread_cond_init(car_to_run_next_condition, NULL) != 0) exit_msg("Condition variables initialization error")
    if (pthread_cond_init(car_availible_condition, NULL) != 0) exit_msg("Condition variables initialization error")
    if (pthread_cond_init(car_to_finish_next_cond, NULL) != 0) exit_msg("Condition variables initialization error")
    if (pthread_mutex_init(car_access_mutex, NULL) != 0) exit_msg("Mutex initialization error")
    if (pthread_mutex_init(car_stop_mutex, NULL) != 0) exit_msg("Mutex initialization error")
    if (pthread_mutex_init(car_finish_mutex, NULL) != 0) exit_msg("Mutex initialization error")

    cars = malloc(car_count * sizeof(Car*));
    car_threads = malloc(car_count * sizeof(pthread_t*));
    passenger_threads = malloc(passenger_count * sizeof(pthread_t*));
    passenger_ids = malloc(passenger_count * sizeof(int));

    atexit(cleanup);
    for (i = 0; i < car_count; i++) {
        cars[i] = new_car(i);
        car_threads[i] = malloc(sizeof(pthread_t));
        pthread_create(car_threads[i], NULL, run_car, (void*) cars[i]);
    }
    for (i = 0; i < passenger_count; i++) {
        passenger_ids[i] = i;
        passenger_threads[i] = malloc(sizeof(pthread_t));
        pthread_create(passenger_threads[i], NULL, run_passenger, (void*) (&passenger_ids[i]));
    }

    for (i = 0; i < car_count; i++) {
        pthread_join(*car_threads[i], NULL);
    }

    for (i = 0; i < passenger_count; i++) {
        pthread_join(*passenger_threads[i], NULL);
    }
}

Car* new_car(int order_number) {
    Car* car = malloc(sizeof(Car));
    car->id = order_number;
    car->current_people_count = 0;
    car->status = 0;
    car->run_number = run_count;
    pthread_mutex_init(&car->access, NULL);
    pthread_cond_init(&car->car_change, NULL);
    pthread_cond_init(&car->passenger_change, NULL);
    return car;
}

void remove_car(Car* car) {
    pthread_cond_destroy(&car->car_change);
    pthread_cond_destroy(&car->passenger_change);
    pthread_mutex_destroy(&car->access);
}

void* run_car(void* arg) {
    Car* car = (Car*) arg;
    while (1) {
        pthread_mutex_lock(car_stop_mutex);
        while (car_to_run_next != car->id) {
            pthread_cond_wait(car_to_run_next_condition, car_stop_mutex);
        }
        pthread_mutex_lock(car_access_mutex);
        current_car = car;
        pthread_mutex_lock(&car->access);
        log("Car %d arrived at car stop", car->id)
        car->status = AT_STOP;
        pthread_cond_broadcast(&car->car_change);
        pthread_mutex_unlock(&car->access);

        pthread_mutex_lock(&car->access);
        while (car->current_people_count != 0) {
            pthread_cond_wait(&car->passenger_change, &car->access);
        }
        car->run_number--;
        if (car->run_number < 0) {
            break;
        }
        car->status = ACCEPTING_PASSENGERS;
        pthread_cond_broadcast(car_availible_condition);
        pthread_mutex_unlock(&car->access);
        pthread_mutex_unlock(car_access_mutex);


        pthread_mutex_lock(&car->access);
        while (car->current_people_count != car_capacity) {
            pthread_cond_wait(&car->passenger_change, &car->access);
        }
        car->status = WAITING_FOR_START;
        pthread_cond_broadcast(&car->car_change);
        pthread_mutex_unlock(&car->access);

        pthread_mutex_lock(&car->access);
        while (car->status != RIDING) {
            pthread_cond_wait(&car->passenger_change, &car->access);
        }
        log("Car %d starts run", car->id)
        car_to_run_next = (car_to_run_next + 1) % car_count;
        pthread_cond_broadcast(car_to_run_next_condition);
        pthread_mutex_unlock(car_stop_mutex);
        pthread_mutex_unlock(&car->access);

        pthread_mutex_lock(car_finish_mutex);
        while (car_to_finish_next != car->id) {
            pthread_cond_wait(car_to_finish_next_cond, car_finish_mutex);
        }
        log("Car %d finishes run", car->id)
        car_to_finish_next = (car_to_finish_next + 1) % car_count;
        pthread_cond_broadcast(car_to_finish_next_cond);
        pthread_mutex_unlock(car_finish_mutex);
    }
    log("Car %d finished work", car->id)
    car_to_run_next = (car_to_run_next + 1) % car_count;
    pthread_cond_broadcast(car_to_run_next_condition);
    pthread_mutex_unlock(car_stop_mutex);
    cars_available--;
    pthread_mutex_unlock(car_access_mutex);
    if (cars_available == 0) {
        pthread_cond_broadcast(car_availible_condition);
    }
    int exit_code = 0;
    pthread_exit((void*) &exit_code);
}

void* run_passenger(void* arg) {
    int i = (int) ((int*) arg)[0];
    Car* my_car;
    srand(time(NULL));
    while (cars_available != 0) {
        pthread_mutex_lock(car_access_mutex);
        while (cars_available != 0 &&
               (current_car == NULL || current_car->status != ACCEPTING_PASSENGERS ||
               current_car->current_people_count == car_capacity)) {
            pthread_cond_wait(car_availible_condition, car_access_mutex);
        }
        if (cars_available == 0) {
            pthread_mutex_unlock(car_access_mutex);
            break;
        }
        my_car = current_car;
        pthread_mutex_lock(&my_car->access);
        while (current_car->status != ACCEPTING_PASSENGERS || current_car->current_people_count == car_capacity) {
            pthread_cond_wait(car_availible_condition, &my_car->access);
        }

        my_car->current_people_count++;
        log("Passenger %d gets on car %d. Currently %d people in car", i, my_car->id, my_car->current_people_count)

        if (my_car->current_people_count == car_capacity) {
            pthread_cond_broadcast(&my_car->passenger_change);
        }
        pthread_mutex_unlock(&my_car->access);

        pthread_mutex_unlock(car_access_mutex);

        pthread_mutex_lock(&my_car->access);
        while ((my_car->status == ACCEPTING_PASSENGERS) && (rand() % 5) < 4) {
            pthread_cond_wait(&my_car->car_change, &my_car->access);
        }
        if (current_car->status == WAITING_FOR_START) {
            my_car->status = RIDING;
            log("Passenger %d pressed the start button in car %d", i, my_car->id)
            pthread_cond_broadcast(&my_car->passenger_change);
        }
        pthread_mutex_unlock(&my_car->access);

        pthread_mutex_lock(&my_car->access);
        while (my_car->status != AT_STOP) {
            pthread_cond_wait(&my_car->car_change, &my_car->access);
        }
        my_car->current_people_count--;
        log("Passenger %d gets off car %d. Currently %d people in car", i, my_car->id, my_car->current_people_count)
        pthread_cond_broadcast(&my_car->passenger_change);
        pthread_mutex_unlock(&my_car->access);
    }
    log("Passenger %d finished riding", i)
    int exit_code = 0;
    pthread_exit((void*) &exit_code);
}

void cleanup() {
    int i;
    for (i = 0; i < car_count; i++) {
        remove_car(cars[i]);
        free(cars[i]);
        free(car_threads[i]);
    }
    for (i = 0; i < passenger_count; i++) {
        free(passenger_threads[i]);
    }
    free(cars);
    free(car_threads);
    free(passenger_threads);
    free(passenger_ids);

    if (pthread_cond_destroy(car_to_run_next_condition) != 0) exit_msg("Condition variables initialization error")
    if (pthread_cond_destroy(car_availible_condition) != 0) exit_msg("Condition variables initialization error")
    if (pthread_cond_destroy(car_to_finish_next_cond) != 0) exit_msg("Condition variables initialization error")
    if (pthread_mutex_destroy(car_access_mutex) != 0) exit_msg("Mutex initialization error")
    if (pthread_mutex_destroy(car_stop_mutex) != 0) exit_msg("Mutex initialization error")
    if (pthread_mutex_destroy(car_finish_mutex) != 0) exit_msg("Mutex initialization error")

    free(car_to_run_next_condition);
    free(car_availible_condition);
    free(car_to_finish_next_cond);
    free(car_access_mutex);
    free(car_stop_mutex);
    free(car_finish_mutex);
}