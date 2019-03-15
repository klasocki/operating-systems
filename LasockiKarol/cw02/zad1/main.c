#include <stdio.h>
#include <zconf.h>
#include <sys/times.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void print_time(clock_t start, clock_t end, struct tms* tms_start, struct tms* tms_end) {
    printf("Real time:    %.9lfs\n", time_diff(start, end));
    printf("User time:    %.12lfs\n", time_diff(tms_start->tms_utime, tms_end->tms_utime));
    printf("Sys. time:    %.12lfs\n\n", time_diff(tms_start->tms_stime, tms_end->tms_stime));
}

char* exec_operation(char* const* argv, int argc, int* i) {
    // returns executed operation name with arguments, for printing
    char* op_executed = "(None)";
    char* op = argv[*i];
    if (strcmp(op, "generate") == 0) { ;
    } else if (strcmp(op, "sort") == 0) { ;
    } else if (strcmp(op, "copy") == 0 || strcmp(op, "create_table") == 0) { ;
    } else {
        fprintf(stderr, "Invalid operation %s\n", op);
    }
    return op_executed;
}

void measure_times(int argc, char* const* argv) {
    int i = 2;
    while (i < argc) {
        struct tms* tms_start_time = malloc(sizeof(struct tms));
        struct tms* tms_end_time = malloc(sizeof(struct tms));
        clock_t start_time = 0;
        clock_t end_time = 0;
        start_time = times(tms_start_time);
        char* op = exec_operation(argv, argc, &i); //exec increments i to get all needed arguments
        ++i; // move to next operation
        end_time = times(tms_end_time);
        printf("Operation: %s\n", op);
        print_time(start_time, end_time, tms_start_time, tms_end_time);
        free(tms_start_time);
        free(tms_end_time);
    }
}

int genetate(char* file_path, int n_lines, size_t n_bytes) {
    FILE* file = fopen(file_path, "w+");
    if (!file) {
        fprintf(stderr, "Trouble opening file %s", file_path);
        return -1;
    }
    time_t t;
    srand((unsigned) time(&t));
    for (int i = 0; i < n_lines; i++) {
        unsigned char tab[n_bytes + 1];
        for (size_t j = 0; j < n_bytes; j++) {
            tab[j] = (unsigned char) rand();
        }
        tab[n_bytes] = (unsigned char) '\n';
        fwrite(tab, sizeof(char), n_bytes + 1, file);
    }
    fclose(file);
    return 0;
}

int main(int argc, char* argv[]) {
//    if (argc < 5) {
//        fprintf(stderr, "Too few arguments, required list of operations, one of:\n"
//                        "generate file_path n_records record_len\n"
//                        "sort file_path n_records record_len function_type\n"
//                        "copy source_path destination_path n_records record_len function_type\n");
//        return -1;
//    }
    genetate("a.txt", 10, 2);
}
