#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/times.h>
#include <zconf.h>
#include <time.h>
#include "find_lib.h"

void measure_times(int argc, char* const* argv, struct result_array* res_arr);

char* exec_operation(char* const* argv, int argc, struct result_array* res_arr, int* i);

char* search_directory(char* const* argv, int argc, struct result_array* res_arr, int* i, const char* op);

char* get_op_call(const char* op, int argument);

char* add_and_delete(char* const* argv, int argc, struct result_array* res_arr, int* i, const char* op);

char* exec_one_arg(char* const* argv, int argc, int* i, const char* op, struct result_array* res_arr);

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void print_time(clock_t start, clock_t end, struct tms* tms_start, struct tms* tms_end) {
    printf("Real time:    %.9lfs\n", time_diff(start, end));
    printf("User time:    %.12lfs\n", time_diff(tms_start->tms_utime, tms_end->tms_utime));
    printf("Sys. time:    %.12lfs\n\n", time_diff(tms_start->tms_stime, tms_end->tms_stime));
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Too few arguments, required: array size, list of operations, one of:\n"
                        "search_directory dir_name file_name file_for_results\n"
                        "add_and_delete how_many_blocks how_many_times file_to_save_as_arr_block\n"
                        "create_table size\n"
                        "remove_block index\n");
        return -1;
    }
    struct result_array* res_arr = create_results_array((int) strtol(argv[1], NULL, 10));
    if (!res_arr) {
        fprintf(stderr, "Error creating array, first argument must be a positive integer\n");
        return -2;
    }
    measure_times(argc, argv, res_arr);
    delete_array(res_arr);
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
        char* op = exec_operation(argv, argc, res_arr, &i); //exec increments i to get all needed arguments
        ++i; // move to next operation
        end_time = times(tms_end_time);
        printf("Operation: %s\n", op);
        print_time(start_time, end_time, tms_start_time, tms_end_time);
        free(tms_start_time);
        free(tms_end_time);
    }
}

char* exec_operation(char* const* argv, int argc, struct result_array* res_arr, int* i) {
    // returns executed operation name with arguments, for printing
    char* op_executed = "(None)";
    char* op = argv[*i];
    if (strcmp(op, "search_directory") == 0) {
        op_executed = search_directory(argv, argc, res_arr, i, op);
    } else if (strcmp(op, "add_and_delete") == 0) {
        op_executed = add_and_delete(argv, argc, res_arr, i, op);
    } else if (strcmp(op, "remove_block_index") == 0 || strcmp(op, "create_table") == 0) {
        op_executed = exec_one_arg(argv, argc, i, op, res_arr);
    } else {
        fprintf(stderr, "Invalid operation %s\n", op);
    }
    return op_executed;
}

char* exec_one_arg(char* const* argv, int argc, int* i, const char* op, struct result_array* res_arr) {
    char* op_executed = NULL;
    if (*i >= argc - 1) {
        fprintf(stderr, "Too few arguments for %s\n", op);
        strcpy(op_executed, op);
        return strcat(op_executed, " (failed)");
    }
    int argument = (int) strtol(argv[++(*i)], NULL, 10);
    if (strcmp(op, "remove_block_index") == 0) {
        if(delete_block(res_arr, argument) < 0){
            fprintf(stderr, "error while removing block %d from main array\n", argument);
            return "remove_block_index (failed)";
        }
    }
    else delete_array(create_results_array(argument));

    op_executed = get_op_call(op, argument);
    return op_executed;
}

char* get_op_call(const char* op, int argument) {
    char* op_executed = calloc(strlen(op) + 50, sizeof(char));
    sprintf(op_executed, "%s %d", op, argument);
    return op_executed;
}

char* search_directory(char* const* argv, int argc, struct result_array* res_arr, int* i, const char* op) {
    if (*i >= argc - 3) {
        fprintf(stderr, "Too few arguments for search_directory\n");
        return "search_directory (failed)";
    }
    char* dir = argv[++(*i)];
    char* filename = argv[++(*i)];
    char* result_tmp_file = argv[++(*i)];
    search(dir, filename, result_tmp_file);
    save_file_in_array(res_arr, result_tmp_file);
    // calculate max length of the op_executed to print
    size_t op_len = strlen(op) + strlen(dir) + strlen(filename) + strlen(result_tmp_file) + 4;
    char* op_executed = calloc(op_len, sizeof(char));
    snprintf(op_executed, op_len, "%s %s %s %s", op, dir, filename, result_tmp_file);
    return op_executed;
}

char* add_and_delete(char* const* argv, int argc, struct result_array* res_arr, int* i, const char* op) {
    if (*i >= argc - 3) {
        fprintf(stderr, "Too few arguments for add_and_delete\n");
        return "add_and_delete (failed)";
    }
    int n_blocks = (int) strtol(argv[++(*i)], NULL, 10);
    int n_times = (int) strtol(argv[++(*i)], NULL, 10);
    char* tmp_file_path = argv[++(*i)];
    for (int i = 0; i < n_times; i++) {
        for (int j = 0; j < n_blocks; ++j) {
            int code = save_file_in_array(res_arr, tmp_file_path);
            if (code < 0) {
                fprintf(stderr, "Error saving file %s to array", tmp_file_path);
                if (code == -2) {
                    fprintf(stderr, ", out of space\n");
                } else if (code == -3) {
                    fprintf(stderr, ", file not existing or forbidden\n");
                }
                fprintf(stderr, "\n");
                return "add_and_delete (failed)";
            }
        }
        for (int j = 0; j < n_blocks; ++j) {
            delete_block(res_arr, res_arr->free_index - 1);
        }
    }
    char* op_executed = calloc(strlen(op) + strlen(tmp_file_path) + 50, sizeof(char));
    sprintf(op_executed, "%s (n_blocks) %d (n_times) %d %s", op, n_blocks, n_times, tmp_file_path);
    return op_executed;
}
