//
// Created by kariok on 07.03.19.
//

#ifndef SYSOPY_FIND_LIB_H
#define SYSOPY_FIND_LIB_H

struct result_array{
    int size;
    int free_index;
    char** array;
};

struct result_array* create_results_array(int n_elements);

void search(char* directory, char* file, char* result_file_path);

int save_file_in_array(struct result_array* array, char* file_path);

int delete_block(struct result_array* array, int index);

void delete_array(struct result_array* array);

#endif //SYSOPY_FIND_LIB_H

