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
#include <zconf.h>


int walk(char* dir_path, char* relative_path) {
    DIR* dir;
    struct stat stat;
    struct dirent* ent;
    if (!(dir = opendir(dir_path))) {
        fprintf(stderr, "Directory %s does not exist, is forbidden, or is not a directory\n", dir_path);
        return -1;
    }
    if (strcmp(relative_path, "") != 0) {
        pid_t pid = vfork();
        if (pid == 0) {
            printf("Process PID: %d\nRelative path: %s\n", getpid(), relative_path);
            char* command = malloc((strlen(dir_path) + 7) * sizeof(char));
            sprintf(command, "ls -l %s", dir_path);
            system(command);
            exit(0);
        }
    }

    while ((ent = readdir(dir))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        char* sub_path = malloc((strlen(dir_path) + strlen(ent->d_name) + 2) * sizeof(char));
        sprintf(sub_path, "%s/%s", dir_path, ent->d_name);
        if (lstat(sub_path, &stat) == -1) {
            fprintf(stderr, "Error while analizing %s\n", sub_path);
            continue;
        }
        if (S_ISDIR(stat.st_mode)) {
            char* buffer = malloc((strlen(relative_path) + strlen(ent->d_name) + 2) * sizeof(char));
            sprintf(buffer, "%s%s/", relative_path, ent->d_name);
            walk(sub_path, buffer);
        }
    }
    if (closedir(dir) != 0) {
        fprintf(stderr, "Error while closing %s\n", dir_path);
        return -2;
    }
    return 0;
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Too few arguments, required: directory");
        return -1;
    }
    char* dir_path = realpath(argv[1], NULL);
    if (!dir_path) {
        fprintf(stderr, "Invalid path %s\n", dir_path);
        return -2;
    }
    walk(dir_path, "");
    return 0;
}