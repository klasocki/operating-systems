#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_LINE_LEN 4096
#define MAX_LINES 64
#define MAX_COMMANDS 16
#define MAX_ARGUMENTS 8

typedef struct {
    char* name;
    char** argv;
} Command;

typedef struct {
    Command* list;
    int length;
} CommandList;

typedef struct {
    char** lines;
    int length;
} Lines;

Lines read_file(char*);

CommandList get_command_list(char* string);

Command parse_command(char* string);

void pipe_commands(CommandList command_list);

void exit_errno(void);

void exit_msg(char*);

int main(int argc, char* argv[]) {
    if (argc != 2)
        exit_msg("Required 1 argument: path to file with commands to execute)");

    Lines command_strings = read_file(argv[1]);
    for (int i = 0; i < command_strings.length; i++) {
        CommandList commands = get_command_list(command_strings.lines[i]);
        pipe_commands(commands);

        for (int i = 0; i < commands.length; i++)
            wait(NULL);
    }

    return 0;
}

Lines read_file(char* file_name) {
    FILE* file = fopen(file_name, "r");
    if (!file) exit_errno();

    char** list = calloc(MAX_LINES, sizeof(char*));
    char* buffer = calloc(MAX_LINE_LEN, sizeof(char));
    int line_count = 0;
    while (fgets(buffer, MAX_LINE_LEN, file)) {
        if (line_count == MAX_LINES) exit_msg("Too many lines in file");
        list[line_count] = calloc(strlen(buffer), sizeof(char));
        strcpy(list[line_count++], buffer);
    }

    Lines lines = {};
    lines.lines = list;
    lines.length = line_count;

    if (fclose(file)) exit_errno();
    free(buffer);

    return lines;
}

CommandList get_command_list(char* string) {
    char* saveptr;
    Command* list = calloc(MAX_COMMANDS, sizeof(Command));
    char* command_str = strtok_r(string, "|\n", &saveptr);
    if (command_str == NULL)
        exit_msg("Each command must at least have a name");

    int i;
    for (i = 0; command_str != NULL; i++, command_str = strtok_r(NULL, "|\n", &saveptr)) {
        if (i < MAX_COMMANDS)
            list[i] = parse_command(command_str);
        else
            exit_msg("Too many commands");
    }

    CommandList commands = {};
    commands.list = list;
    commands.length = i;

    return commands;
}

Command parse_command(char* string) {
    char* saveptr;
    char** args = calloc(MAX_ARGUMENTS, sizeof(char*));
    Command command = {};

    char* arg = strtok_r(string, "\n \t", &saveptr);

    if (arg == NULL) {
        exit_msg("Each command must at least have a name");
    }

    for (int i = 0; arg != NULL; i++, arg = strtok_r(NULL, "\n \t", &saveptr)) {
        if (i == 0)
            command.name = arg;
        if (i < MAX_ARGUMENTS)
            args[i] = arg;
        else
            exit_msg("Too many arguments");
    }

    command.argv = args;
    return command;
}

void pipe_commands(CommandList command_list) {
    int pipes[2][2];

    int i = 0;
    for (i = 0; i < command_list.length; i++) {
        Command cmd = command_list.list[i];

        if (i) {
            close(pipes[i % 2][0]);
            close(pipes[i % 2][1]);
        }

        if (pipe(pipes[i % 2]) == -1) exit_errno();
        pid_t is_parent = fork();
        if (!is_parent) {
            if (i < command_list.length - 1) { // last pipe should output to STDOUT, otherwise write to pipe instead
                close(pipes[i % 2][0]);
                if (dup2(pipes[i % 2][1], STDOUT_FILENO) == -1)
                    exit_errno();
            }
            if (i != 0) { // close previous pipe writing end, and read form it instead of STDIN
                close(pipes[(i + 1) % 2][1]);
                if (dup2(pipes[(i + 1) % 2][0], STDIN_FILENO) == -1)
                    exit_errno();
            }
            execvp(cmd.name, cmd.argv);
        }
    }
    close(pipes[i % 2][0]);
    close(pipes[i % 2][1]);
}


void exit_errno(void) {
    exit_msg(strerror(errno));
}

void exit_msg(char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}