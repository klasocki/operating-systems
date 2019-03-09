#include <dlfcn.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <zconf.h>
#include <time.h>

#ifndef DLL

#include "find_lib.h"

#endif

#ifdef DLL
void* dll_handle;
struct result_array{
    int size;
    int free_index;
    char** array;
};

#endif


void measure_times(int argc, char* const* argv, struct result_array* res_arr);

char* exec_operation(char* const* argv, struct result_array* res_arr, int* i);

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void print_time(clock_t start, clock_t end, struct tms* tms_start, struct tms* tms_end) {
    printf("Real time:    %.9lfs\n", time_diff(start, end));
    printf("User time:    %.12lfs\n", time_diff(tms_start->tms_utime, tms_end->tms_utime));
    printf("Sys. time:    %.12lfs\n\n", time_diff(tms_start->tms_stime, tms_end->tms_stime));
}


int main(int argc, char* argv[]) {
#ifdef DLL
    dll_handle = dlopen("./libfind_lib.so", RTLD_LAZY);
    struct result_array* (*create_results_array) (int) = dlsym(dll_handle, "create_results_array");
    void* (*delete_array) (struct result_array*) = dlsym(dll_handle, "delete_array");
#endif
    if (argc < 3) return -1;
    struct result_array* res_arr = create_results_array((int) strtol(argv[1], NULL, 10));
    measure_times(argc, argv, res_arr);
    delete_array(res_arr);
#ifdef DLL
    dlclose(dll_handle);
#endif
    return 0;
}

void measure_times(int argc, char* const* argv, struct result_array* res_arr) {
    int i = 2;
    while (i < argc) {
        struct tms* tms_start_time = malloc(sizeof(struct tms));
        struct tms* tms_end_time = malloc(sizeof(struct tms));
        clock_t start_time = 0;
        clock_t end_time = 0;
        start_time = times(tms_start_time);
        char* op = exec_operation(argv, res_arr, &i);
        end_time = times(tms_end_time);
        printf("Operation: %s\n", op);
        print_time(start_time, end_time, tms_start_time, tms_end_time);
        free(tms_start_time);
        free(tms_end_time);
    }
}

char* exec_operation(char* const* argv, struct result_array* res_arr, int* i) {
#ifdef DLL
    void* (*search) (char*, char*, char*) = dlsym(dll_handle, "search");
    int* (*save_file_in_array) (struct result_array*, char*) = dlsym(dll_handle, "save_file_in_array");
    int* (*delete_block) (struct result_array*, int) = dlsym(dll_handle, "delete_block");
    struct result_array* (*create_results_array) (int) = dlsym(dll_handle, "create_results_array");
#endif
    // returns executed operation name with arguments, to print later
    char* result = "(None)";
    char* op = argv[*i];
    if (strcmp(op, "search_directory") == 0) {
        char* dir = argv[++(*i)];
        char* filename = argv[++(*i)];
        char* result_tmp_file = argv[++(*i)];
        search(dir, filename, result_tmp_file);
        save_file_in_array(res_arr, result_tmp_file);
        // calculate max length of the result to print
        size_t op_len = strlen(op) + strlen(dir) + strlen(filename) + strlen(result_tmp_file) + 4;
        result = calloc(op_len, sizeof(char));
        snprintf(result, op_len, "%s %s %s %s", op, dir, filename, result_tmp_file);
    } else if (strcmp(op, "remove_block_index") == 0) {
        int index = (int) strtol(argv[++(*i)], NULL, 10);
        delete_block(res_arr, index);
        result = calloc(strlen(op) + 50, sizeof(char));
        sprintf(result, "%s %d", op, index);
    } else if (strcmp(op, "create_table") == 0) {
        int size = (int) strtol(argv[++(*i)], NULL, 10);
        create_results_array(size);
        result = calloc(strlen(op) + 50, sizeof(char));
        sprintf(result, "%s %d", op, size);
    } else if (strcmp(op, "add_and_delete") == 0) {
        int n_blocks = (int) strtol(argv[++(*i)], NULL, 10);
        int n_times = (int) strtol(argv[++(*i)], NULL, 10);
        char* tmp_file_path = argv[++(*i)];
        for (int i = 0; i < n_times; i++) {
            for (int j = 0; j < n_blocks; ++j) {
                save_file_in_array(res_arr, tmp_file_path);
            }
            for (int j = 0; j < n_blocks; ++j) {
                delete_block(res_arr, res_arr->free_index - 1);
            }
        }
        result = calloc(strlen(op) + strlen(tmp_file_path) + 50, sizeof(char));
        sprintf(result, "%s (n_blocks) %d (n_times) %d %s", op, n_blocks, n_times, tmp_file_path);
    } else {
        printf("Invalid operation %s\n", op);
    }
    (*i)++;
    return result;
}
