#include <stddef.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <assert.h>
#include "find_lib.h"


struct result_array* create_results_array(int n_elements) {
    if (n_elements <= 0) {
        return NULL;
    }
    struct result_array* result = malloc(sizeof(struct result_array));
    result->free_index = 0;
    result->size = n_elements;
    result->array = calloc((size_t) n_elements, sizeof(char*));
    return result;
}

void search(char* directory, char* file, char* result_file_path) {
    size_t max_comm_len = strlen(directory) + strlen(file) + strlen(result_file_path) + 20;
    char command[max_comm_len];
    snprintf(command, max_comm_len, "find %s -name %s >> %s", directory, file, result_file_path);
    system(command);
}

size_t get_txt_file_len(FILE* file) {
    size_t pos = (size_t) ftell(file); //save current position
    fseek(file, 0, SEEK_END);
    size_t file_length = (size_t) ftell(file);
    fseek(file, pos, SEEK_SET); //restore previous position
    return file_length;
}

int save_file_in_array(struct result_array* array, char* file_path) {
    if (!array || !file_path) return -1;
    if (array->free_index >= array->size) return -2;
    FILE* file;
    char c;
    int i;

    file = fopen(file_path, "r");
    if (!file) return -3;
    size_t file_length = get_txt_file_len(file);
    char* block = calloc(file_length + 1, sizeof(char));

    i = 0;
    while ((c = (char) fgetc(file)) != EOF) {
        block[i] = c;
        i++;
    }
    block[i] = '\0';
    array->array[array->free_index] = block;
    array->free_index++;
    fclose(file);
    return array->free_index - 1;
}

int delete_block(struct result_array* array, int index) {
    if (!array) return -1;
    if (index >= array->size) return -2;
    if(array->array[index] == NULL) return 0;
    free(array->array[index]);
    array->array[index] = NULL;
    //if deleted block was last, we can move free_index back
    if (index == array->free_index - 1) {
        array -> free_index -= 1;
    }
    return 0;
}

void delete_array(struct result_array* array) {
    if (!array) return;
    for (int i = 0; i < array->size; i++) {
        if (array->array[i] != NULL)
            free(array->array[i]);
    }
    free(array);
}

