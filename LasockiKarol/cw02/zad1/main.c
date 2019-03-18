#include <stdio.h>
#include <zconf.h>
#include <sys/times.h>
#include <memory.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>

char* execute_generate(char* const* argv, int* i);

char* execute_sort(char* const* argv, int* i);

char* execute_copy(char* const* argv, int* i);

double time_diff(clock_t start, clock_t end) {
    return (double) (end - start) / sysconf(_SC_CLK_TCK);
}

void print_time(clock_t start, clock_t end, struct tms* tms_start, struct tms* tms_end) {
    printf("Real time:    %.9lfs\n", time_diff(start, end));
    printf("User time:    %.12lfs\n", time_diff(tms_start->tms_utime, tms_end->tms_utime));
    printf("Sys. time:    %.12lfs\n\n", time_diff(tms_start->tms_stime, tms_end->tms_stime));
}

int generate(char* file_path, int n_lines, size_t n_bytes) {
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
        if (fread(buffer1, sizeof(char), line_len, file) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    file_path, n_bytes);
            return -2;
        }
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
        if (fread(buffer2, sizeof(char), line_len, file) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    file_path, n_bytes);
            return -2;
        }
        fseek(file, min_index * line_len, SEEK_SET);
        fwrite(buffer1, sizeof(char), line_len, file);
        fseek(file, i * line_len, SEEK_SET);
        fwrite(buffer2, sizeof(char), line_len, file);
        if (ferror(file)) {
            fprintf(stderr, "Error writing to file %s", file_path);
            return -3;
        }
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
        if (read(file, buffer1, line_len * sizeof(char)) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    file_path, n_bytes);
            return -2;
        }
        unsigned char min = buffer1[0];
        int min_index = i;
        for (int j = 0; j < n_lines; j++) {
            lseek(file, j * line_len, SEEK_SET);
            unsigned char first_in_line;
            if (read(file, &first_in_line, sizeof(char)) != 1) {
                fprintf(stderr, "Error while reading file %s",
                        file_path);
                return -2;
            }
            if (first_in_line < min) {
                min = first_in_line;
                min_index = j;
            }
        }
        lseek(file, min_index * line_len, SEEK_SET);
        if (read(file, buffer2, line_len * sizeof(char)) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    file_path, n_bytes);
            return -2;
        }
        lseek(file, min_index * line_len, SEEK_SET);
        if (write(file, buffer1, line_len * sizeof(char)) == -1) {
            fprintf(stderr, "Error writing to file %s",
                    file_path);
            return -3;
        }
        lseek(file, i * line_len, SEEK_SET);
        if (write(file, buffer2, line_len * sizeof(char)) == -1) {
            fprintf(stderr, "Error writing to file %s",
                    file_path);
            return -3;
        }
    }
    if (close(file) != 0) {
        fprintf(stderr, "Error while closing file %s", file_path);
        return -4;
    }
    return 0;
}

int copy_lib(char* source_path, char* dest_path, int n_lines, int n_bytes) {
    size_t line_len = (size_t) (n_bytes + 1);
    FILE* source_file = fopen(source_path, "r");
    FILE* dest_file = fopen(dest_path, "w");
    if (!source_file || !dest_file) {
        fprintf(stderr, "Trouble opening file %s or %s", source_path, dest_path);
        return -1;
    }
    unsigned char buffer[line_len];
    for (int i = 0; i < n_lines; i++) {
        if (fread(buffer, sizeof(char), line_len, source_file) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    source_path, n_bytes);
            return -2;
        }
        fwrite(buffer, sizeof(char), line_len, dest_file);
    }
    if (ferror(dest_file)) {
        fprintf(stderr, "Error writing to file %s", dest_path);
        return -3;
    }
    if (fclose(source_file) != 0) {
        fprintf(stderr, "Error closing file %s", source_path);
        return -4;
    }
    if (fclose(dest_file) != 0) {
        fprintf(stderr, "Error closing file %s", dest_path);
        return -5;
    }
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
        if (read(source_file, buffer, line_len * sizeof(char)) != line_len) {
            fprintf(stderr, "Error while reading file %s, each line must contain exactly %d bytes",
                    source_path, n_bytes);
            return -2;
        }
        if (write(dest_file, buffer, line_len * sizeof(char)) == -1) {
            fprintf(stderr, "Error writing to file %s",
                    dest_path);
            return -3;
        }
    }
    if (close(source_file) != 0) {
        fprintf(stderr, "Error while closing file %s", source_path);
        return -4;
    }
    if (close(dest_file) != 0) {
        fprintf(stderr, "Error while closing file %s", dest_path);
        return -5;
    }
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
            op_executed = execute_generate(argv, i);
        }
    } else if (strcmp(op, "sort") == 0) {
        if (argc - (*i) < 5) {
            fprintf(stderr, "Too few arguments to sort, 4 required\n");
        } else {
            op_executed = execute_sort(argv, i);
        }
    } else if (strcmp(op, "copy") == 0) {
        if (argc - (*i) < 6) {
            fprintf(stderr, "Too few arguments to copy, 5 required\n");
        } else {
            op_executed = execute_copy(argv, i);
        }
    } else {
        fprintf(stderr, "Invalid operation %s\n", op);
    }
    return op_executed;
}

char* execute_copy(char* const* argv, int* i) {
    char* op_executed;
    char* source_path = argv[++(*i)];
    char* dest_path = argv[++(*i)];
    int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
    int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
    char* type = argv[++(*i)];
    if (strcmp(type, "lib") == 0) {
        if (copy_lib(source_path, dest_path, n_lines, n_bytes) != 0) return "(None)";
        char operation[100 + strlen(source_path) + strlen(dest_path)];
        snprintf(operation, 100 + strlen(source_path) + strlen(dest_path),
                 "copy %s %s %d %d lib", source_path, dest_path, n_lines, n_bytes);
        op_executed = operation;
    } else if (strcmp(type, "sys") == 0) {
        if (copy_sys(source_path, dest_path, n_lines, n_bytes) != 0) return "(None)";
        char operation[100 + strlen(source_path) + strlen(dest_path)];
        snprintf(operation, 100 + strlen(source_path) + strlen(dest_path),
                 "copy %s %s %d %d sys", source_path, dest_path, n_lines, n_bytes);
        op_executed = operation;
    } else {
        fprintf(stderr, "Invalid type %s, must be either lib or sys", type);
        op_executed = "(None)";
    }
    return op_executed;
}

char* execute_sort(char* const* argv, int* i) {
    char* op_executed;
    char* file_path = argv[++(*i)];
    int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
    int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
    char* type = argv[++(*i)];
    if (strcmp(type, "lib") == 0) {
        if (sort_lib(file_path, n_lines, n_bytes) != 0) return "(None)";
        char operation[100 + strlen(file_path)];
        snprintf(operation, 100 + strlen(file_path), "sort %s %d %d lib", file_path, n_lines, n_bytes);
        op_executed = operation;
    } else if (strcmp(type, "sys") == 0) {
        if (sort_sys(file_path, n_lines, n_bytes) != 0) return "(None)";
        char operation[100 + strlen(file_path)];
        snprintf(operation, 100 + strlen(file_path), "sort %s %d %d sys", file_path, n_lines, n_bytes);
        op_executed = operation;
    } else {
        fprintf(stderr, "Invalid type %s, must be either lib or sys\n", type);
        op_executed = "(None)";
    }
    return op_executed;
}

char* execute_generate(char* const* argv, int* i) {
    char* op_executed;
    char* file_path = argv[++(*i)];
    int n_lines = (int) strtol(argv[++(*i)], NULL, 10);
    int n_bytes = (int) strtol(argv[++(*i)], NULL, 10);
    if (generate(file_path, n_lines, (size_t) n_bytes) != 0) return "(None)";
    char operation[100 + strlen(file_path)];
    snprintf(operation, 100 + strlen(file_path), "generate %s %d %d", file_path, n_lines, n_bytes);
    op_executed = operation;
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
                        "execute_generate file_path n_records record_len\n"
                        "sort file_path n_records record_len function_type (lib or sys)\n"
                        "copy source_path destination_path n_records record_len function_type (lib or sys)\n");
        return -1;
    }
    measure_times(argc, argv);
    return 0;
}
