#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int val_from_range(int val, int left, int right){
    if (left < val) {
        return (val < right) ? val : right;
    }
    return left;
}

typedef struct Image {
    int width;
    int height;
    int** values;
} Image;

typedef struct Filter {
    int size;
    double** values;
} Filter;

Image* input_img, * output_img;

Filter* filter;
int thread_count;

static void* run_block(void* init_val);

static void* run_interleaved(void* init_val);

Filter* read_filter(char* filter_path);

void filter_pixel(int y, int x);

void filter_column(int j);

Image* allocate_image(int width, int height);

Image* create_empty_image(int width, int height);

Image* read_image(char* image_path);

void save_image(Image* img, char* save_path);

void exit_errno();

void exit_msg(char* msg);

double get_time(struct timeval tm) {
    return (double) tm.tv_sec + (double) tm.tv_usec / 1e6;
}

struct timeval get_curr_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t;
}

double time_diff(struct timeval start) {
    return get_time(get_curr_time()) - get_time(start);
}


int main(int argc, char* argv[]) {
    if (argc < 6)
        exit_msg("Required arguments: thread number, threading type (block or interleaved),"
                 " input_img path, filter path, output_img path");

    thread_count = atoi(argv[1]);
    char* type = argv[2];
    char* input_path = argv[3];
    char* filter_path = argv[4];
    char* output_path = argv[5];

    input_img = read_image(input_path);
    filter = read_filter(filter_path);
    output_img = create_empty_image(input_img->width, input_img->height);

    pthread_t* thread_ids = malloc(thread_count * sizeof(pthread_t));
    for (int i = 0; i < thread_count; i++) {
        int* thread_index = malloc(sizeof(int));
        *thread_index = i;
        if (strcmp("block", type) == 0)
            pthread_create(thread_ids + i, NULL, &run_block, thread_index);
        else
            pthread_create(thread_ids + i, NULL, &run_interleaved, thread_index);
    }

    struct timeval start = get_curr_time();
    printf("Thread execution times:\n");
    for (int k = 0; k < thread_count; k++) {
        double* time;
        pthread_join(thread_ids[k], (void*) &time);
        printf("Number %d:\t%.6lfs\n", k + 1, *time);
    }
    printf("Total time taken:\t%.6lfs\n", time_diff(start));

    save_image(output_img, output_path);

    return 0;
}

static void* run_block(void* init_val) {
    struct timeval start_time = get_curr_time();

    int k = (*(int*) init_val);

    int start_column = ceil(k * input_img->width / (double) thread_count);
    int end_column = ceil((k + 1) * input_img->width / (double) thread_count);
    for (int i = start_column; i < end_column; i++) {
        filter_column(i);
    }

    double* time = malloc(sizeof(double));
    *time = time_diff(start_time);

    pthread_exit(time);
}

static void* run_interleaved(void* init_val) {
    struct timeval start_time = get_curr_time();

    int k = (*(int*) init_val);
    for (int i = k; i < input_img->width; i += thread_count) {
        filter_column(i);
    }

    double* time = malloc(sizeof(double));
    *time = time_diff(start_time);

    pthread_exit(time);
}

void filter_pixel(int y, int x) {
    int c = filter->size;
    double pixel = 0;
    for (int i = 0; i < c; i++) {
        for (int j = 0; j < c; j++) {
            int y1_val = x - ceil(c / 2.0) + i - 1;
            int x1_val = y - ceil(c / 2.0) + j - 1;
            int y1 = val_from_range(y1_val, 0, input_img->height - 1);
            int x1 = val_from_range(x1_val, 0, input_img->width - 1);
            pixel += input_img->values[y1][x1] * filter->values[i][j];
        }
    }
    output_img->values[x][y] = round(pixel);
}

void filter_column(int j) {
    for (int i = 0; i < input_img->height; i++)
        filter_pixel(j, i);
}

Filter* read_filter(char* filter_path) {
    FILE* file = fopen(filter_path, "r");
    if (file == NULL) exit_errno();

    int size;
    fscanf(file, "%d", &size);

    Filter* filter = malloc(sizeof(Filter));
    filter->size = size;
    filter->values = malloc(filter->size * sizeof(double*));

    for (int i = 0; i < filter->size; i++) {
        filter->values[i] = malloc(filter->size * sizeof(double));
    }

    for (int x = 0; x < filter->size; x++) {
        for (int y = 0; y < filter->size; y++) {
            fscanf(file, "%lf", &filter->values[x][y]);
        }
    }

    fclose(file);
    return filter;
}

Image* allocate_image(int width, int height) {
    Image* img = malloc(sizeof(Image));

    img->width = width;
    img->height = height;

    img->values = malloc(img->height * sizeof(int*));
    for (int i = 0; i < img->height; i++)
        img->values[i] = malloc(img->width * sizeof(int));

    return img;
}

Image* create_empty_image(int width, int height) {
    Image* img = allocate_image(width, height);
    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            img->values[i][j] = 0;
        }
    }
    return img;
}

Image* read_image(char* image_path) {
    FILE* file = fopen(image_path, "r");
    if (file == NULL) exit_errno();

    int width, height;

    fscanf(file, "P2 %d %d 255", &width, &height);

    Image* img = allocate_image(width, height);

    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            fscanf(file, "%d", &img->values[i][j]);
        }
    }

    fclose(file);

    return img;
}

void save_image(Image* img, char* save_path) {
    FILE* file = fopen(save_path, "w");
    if (file == NULL) exit_errno();

    fprintf(file, "P2\n%d %d\n255\n", img->width, img->height);

    for (int i = 0; i < img->height; i++) {
        for (int j = 0; j < img->width; j++) {
            fprintf(file, "%d", img->values[i][j]);
            if (j + 1 != img->width) fputc(' ', file);
        }
        fputc('\n', file);
    }
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

void exit_errno() {
    exit_msg(strerror(errno));
}