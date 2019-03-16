#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <memory.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <ftw.h>
#include <stdlib.h>

time_t TIME_TO_COMPARE;
char GIVEN_OPERATOR;
const char* TIME_FORMAT = "%Y-%m-%d %H:%M:%S";

time_t parse_time(char* t){
    struct tm time;
    char* res = strptime(t, TIME_FORMAT, &time);
    if(res == NULL || *res != '\0'){
        fprintf(stderr, "Incorrect date format, use YYYY-MM-DD hh:mm:ss, for example 2000-01-31 13:54\n");
        return 0;
    }
    return mktime(&time);
}

int compare_time(time_t file_time, time_t time, char operator){
    if (operator == '=') {
        return file_time == time;
    }
    if (operator == '>') {
        return file_time > time;
    }
    if (operator == '<') {
        return file_time < time;
    }
    fprintf(stderr, "Invalid operator %c\n", operator);
    return 0;
}

void print_file_info(const char* path, const struct stat* stat) {
    assert(path && stat);
    printf("Ścieżka: %s\n", path);
    printf("Typ: ");
    if (S_ISREG(stat->st_mode))
        printf("Zwykły plik\n");
    else if (S_ISDIR(stat->st_mode))
        printf("Katalog\n");
    else if (S_ISCHR(stat->st_mode))
        printf("Urządzenie znakowe\n");
    else if (S_ISBLK(stat->st_mode))
        printf("Urzączenie blokowe\n");
    else if (S_ISFIFO(stat->st_mode))
        printf("Potok nazwany\n");
    else if (S_ISLNK(stat->st_mode))
        printf("Link symboliczny\n");
    else
        printf("Soket\n");
    printf("Rozmiar w bajtach: %ld\n", stat->st_size);
    char* buffer = malloc(30);
    strftime(buffer, 30, TIME_FORMAT, localtime(&stat->st_atime));
    printf("Data ostatniego dostępu: %s\n", buffer);
    strftime(buffer, 30, TIME_FORMAT, localtime(&stat->st_mtime));
    printf("Data ostatniej modyfikacji: %s\n", buffer);
    free(buffer);
    printf("__________________________________________\n");
}

int walk(char* dir_path, char operator, time_t time){
    DIR* dir;
    struct stat stat;
    struct dirent* ent;
    if (!(dir = opendir(dir_path))) {
        fprintf(stderr, "Directory %s does not exist, is forbidden, or is not a directory\n", dir_path);
        return -1;
    }
    while ((ent = readdir(dir))) {
        if(strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char* sub_path = malloc((strlen(dir_path) + strlen(ent->d_name) + 2)* sizeof(char));
        sprintf(sub_path, "%s/%s", dir_path, ent->d_name);
        if(lstat(sub_path, &stat) == -1) {
            fprintf(stderr, "Error while analizing %s\n", sub_path);
            continue;
        }
        if (compare_time(stat.st_mtime, time, operator)) {
            print_file_info(sub_path, &stat);
        }
        if (S_ISDIR(stat.st_mode)) {
            walk(sub_path, operator, time);
        }
    }
    closedir(dir);
    return 0;
}

int nftw_walk(const char* path, const struct stat* stat, int typeflag, struct FTW* ftwbuf){
    if(ftwbuf->level != 0 && compare_time(stat->st_mtime, TIME_TO_COMPARE, GIVEN_OPERATOR)){
        print_file_info(path, stat);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Too few arguments, required: directory, ( >, < or =),"
                        " date in format YYYY-DD-MM hh:mm:ss (inside quotation marks)"
                        " and type of function to use (nftw or stat)\n");
        return -1;
    }
    char* dir_path = realpath(argv[1], NULL);
    if(!dir_path) {
        fprintf(stderr, "Invalid path %s", dir_path);
        return -2;
    }
    char operator = argv[2][0];
    char* date = argv[3];
    char* type = argv[4];
    if (strcmp(type, "stat") == 0) {
        walk(dir_path, operator, parse_time(date));
    } else if (strcmp(type, "nftw") == 0) {
        TIME_TO_COMPARE = parse_time(date);
        GIVEN_OPERATOR = operator;
        nftw(dir_path, nftw_walk, 30, FTW_PHYS);
    } else {
        fprintf(stderr, "Illegal type, must be either nftw or stat\n");
        return -3;
    }
    return 0;
}