#include <stdio.h>
#include <zconf.h>
#include <sys/times.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void print_time(clock_t start, clock_t end, struct tms* tms_start, struct tms* tms_end) {
    printf("Real time:    %.9lfs\n", time_diff(start, end));
    printf("User time:    %.12lfs\n", time_diff(tms_start->tms_utime, tms_end->tms_utime));
    printf("Sys. time:    %.12lfs\n\n", time_diff(tms_start->tms_stime, tms_end->tms_stime));
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

int sort_lib(char* file_path, int n_lines, int n_bytes) {
    size_t line_len = (size_t) (n_bytes + 1);
    FILE* file = fopen(file_path, "r+");
    if (!file) {
        fprintf(stderr, "Trouble opening file %s", file_path);
        return -1;
    }
    unsigned char buffer1[line_len];
    unsigned char buffer2[line_len];
    for (int i = 0; i < n_lines; i++) {
        fseek(file, i * line_len, SEEK_SET);
        fread(buffer1, sizeof(char), (size_t) line_len, file);
        unsigned char min = buffer1[0];
        int min_index = i;
        for (int j = i + 1; j < n_lines; j++) {
            fseek(file, j * line_len, SEEK_SET);
            unsigned char first_in_line = (unsigned char) fgetc(file);
            if (first_in_line < min) {
                min = first_in_line;
                min_index = j;
            }
        }
        fseek(file, min_index * line_len, SEEK_SET);
        fread(buffer2, sizeof(char), line_len, file);
        fseek(file, min_index * line_len, SEEK_SET);
        fwrite(buffer1, sizeof(char), line_len, file);
        fseek(file, i * line_len, SEEK_SET);
        fwrite(buffer2, sizeof(char), line_len, file);
    }
    fclose(file);
    return 0;
}

int sort_sys(char* file_path, int n_lines, int n_bytes) {
    int line_len = n_bytes + 1;
    int file = open(file_path, O_RDWR);
    if (!file) {
        fprintf(stderr, "Trouble opening file %s", file_path);
        return -1;
    }
    unsigned char buffer1[line_len];
    unsigned char buffer2[line_len];
    for (int i = 0; i < n_lines; i++) {
        lseek(file, i * line_len, SEEK_SET);
        read(file, buffer1, line_len * sizeof(char));
        unsigned char min = buffer1[0];
        int min_index = i;
        for (int j = 0; j < n_lines; j++) {
            lseek(file, j * line_len, SEEK_SET);
            unsigned char first_in_line;
            read(file, &first_in_line, sizeof(char));
            if (first_in_line < min) {
                min = first_in_line;
                min_index = j;
            }
        }
        lseek(file, min_index * line_len, SEEK_SET);
        read(file, buffer2, line_len * sizeof(char));
        lseek(file, min_index * line_len, SEEK_SET);
        write(file, buffer1, line_len * sizeof(char));
        lseek(file, i * line_len, SEEK_SET);
        write(file, buffer2, line_len * sizeof(char));
    }
    close(file);
    return 0;
}

int copy_lib(char* source_path, char* dest_path, int n_lines, int n_bytes) {
    int line_len = n_bytes + 1;
    FILE* source_file = fopen(source_path, "r");
    FILE* dest_file = fopen(dest_path, "w");
    if (!source_file || !dest_file) {
        fprintf(stderr, "Trouble opening file %s or %s", source_path, dest_path);
        return -1;
    }
    unsigned char buffer[line_len];
    for (int i = 0; i < n_lines; i++) {
        fread(buffer, sizeof(char), (size_t) line_len, source_file);
        fwrite(buffer, sizeof(char), (size_t) line_len, dest_file);
    }
    fclose(source_file);
    fclose(dest_file);
    return 0;
}

int copy_sys(char* source_path, char* dest_path, int n_lines, int n_bytes) {
    size_t line_len = (size_t) (n_bytes + 1);
    int source_file = open(source_path, O_RDONLY);
    int dest_file = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC);
    if (!source_file || !dest_file) {
        fprintf(stderr, "Trouble opening file %s or %s", source_path, dest_path);
        return -1;
    }
    unsigned char buffer[line_len];
    for (int i = 0; i < n_lines; i++) {
        read(source_file, buffer, line_len);
        write(dest_file, buffer, line_len);
    }
    close(source_file);
    close(dest_file);
    return 0;
}

char* exec_operation(char* const* argv, int argc, int* i) {
    // returns executed operation name with arguments, for printing
    char* op_executed = "(None)";
    char* op = argv[*i];
    if (strcmp(op, "generate") == 0) {
        if (argc - (*i) < 4) {
            fprintf(stderr, "Too few arguments to generate, 3 required\n");
        } else {
            char* file_path = argv[++(*i)];
            int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
            int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
            genetate(file_path, n_lines, (size_t) n_bytes);
            char operation[100 + strlen(file_path)];
            snprintf(operation, 100 + strlen(file_path), "generate %s %d %d", file_path, n_lines, n_bytes);
            op_executed = operation;
        }
    } else if (strcmp(op, "sort") == 0) {
        if (argc - (*i) < 5) {
            fprintf(stderr, "Too few arguments to sort, 4 required\n");
        } else {
            char* file_path = argv[++(*i)];
            int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
            int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
            char* type = argv[++(*i)];
            if (strcmp(type, "lib") == 0) {
                sort_lib(file_path, n_lines, n_bytes);
                char operation[100 + strlen(file_path)];
                snprintf(operation, 100 + strlen(file_path), "sort %s %d %d lib", file_path, n_lines, n_bytes);
                op_executed = operation;
            } else if (strcmp(type, "sys") == 0) {
                sort_sys(file_path, n_lines, n_bytes);
                char operation[100 + strlen(file_path)];
                snprintf(operation, 100 + strlen(file_path), "sort %s %d %d sys", file_path, n_lines, n_bytes);
                op_executed = operation;
            } else {
                fprintf(stderr, "Invalid type %s, must be either lib or sys\n", type);
            }
        }
    } else if (strcmp(op, "copy") == 0) {
        if (argc - (*i) < 6) {
            fprintf(stderr, "Too few arguments to copy, 5 required\n");
        } else {
            char* source_path = argv[++(*i)];
            char* dest_path = argv[++(*i)];
            int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
            int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
            char* type = argv[++(*i)];
            if (strcmp(type, "lib") == 0) {
                copy_lib(source_path, dest_path, n_lines, n_bytes);
                char operation[100 + strlen(source_path) + strlen(dest_path)];
                snprintf(operation, 100 + strlen(source_path) + strlen(dest_path),
                         "copy %s %s %d %d lib", source_path, dest_path, n_lines, n_bytes);
                op_executed = operation;
            } else if (strcmp(type, "sys") == 0) {
                copy_sys(source_path, dest_path, n_lines, n_bytes);
                char operation[100 + strlen(source_path) + strlen(dest_path)];
                snprintf(operation, 100 + strlen(source_path) + strlen(dest_path),
                         "copy %s %s %d %d sys", source_path, dest_path, n_lines, n_bytes);
                op_executed = operation;
            } else {
                fprintf(stderr, "Invalid type %s, must be either lib or sys", type);
            }
        }
    } else {
        fprintf(stderr, "Invalid operation %s\n", op);
    }
    return op_executed;
}

void measure_times(int argc, char* const* argv) {
    int i = 1;
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

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Too few arguments, required list of operations, one of:\n"
                        "generate file_path n_records record_len\n"
                        "sort file_path n_records record_len function_type\n"
                        "copy source_path destination_path n_records record_len function_type\n");
        return -1;
    }
    measure_times(argc, argv);
    return 0;
}
