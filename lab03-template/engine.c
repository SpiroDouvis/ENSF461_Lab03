#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include "parser.h"

const char* path;

typedef struct variable{
    char* name;
    char* value;
} variable_type;

variable_type** variables;
int variable_count = 0;

int read_line(int infile, char* buffer, int maxlen)
{
    static int file_offset = 0;
    int readlen = 0;

    char ch;
    while (readlen < maxlen - 1)
    {
        int result = pread(infile, &ch, 1, file_offset++);
        if (result == 0)
        { // End of file
            break;
        }
        else if (result < 0)
        { // Error
            return -1;
        }
        buffer[readlen++] = ch;
        if (ch == '\n')
        { // End of line
            break;
        }
    }
    buffer[readlen] = '\0'; // Null-terminate the string

    return readlen;
}

int normalize_executable(char** command)
{
    char* pathcpy = strdup(path);
    char* tokens = strtok(pathcpy, ":");
    while (tokens != NULL)
    {
        char* command_path = malloc(strlen(tokens) + strlen(command[0]) + 2);
        strcpy(command_path, tokens);
        strcat(command_path, "/");
        strcat(command_path, command[0]);
        if (access(command_path, F_OK) == 0)
        {
            command[0] = command_path;
            while ((tokens = strtok(NULL, ":")) != NULL)
            {
            }
            return 1;
        }
        free(command_path);
        tokens = strtok(NULL, ":");
    }
    return 0;
}

void add_variable(const char* name, const char* value) {
    variables = (variable_type**) realloc(variables, sizeof(variable_type*) * (variable_count + 1));
    assert(variables != NULL);

    variables[variable_count] = (variable_type*) malloc(sizeof(variable_type));
    assert(variables[variable_count] != NULL);

    variables[variable_count]->name = strdup(name);
    variables[variable_count]->value = strdup(value);
    variable_count++;
}

void update_variable(char* name, char* value) {
    // Update or create a variable
    if (variables == NULL) {
        add_variable(name, value);
        return;
    }

    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i]->name, name) == 0) {
            variables[i]->value = strdup(value);
            return;
        }
    }

    add_variable(name, value);
}

char* lookup_variable(char* name)
{
    // Lookup a variable

    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variables[i]->name, name) == 0) {
            // Found the variable
            return variables[i]->value;
        }
    }
    return NULL;
}

int main(int argc, char* argv[])
{
    path = getenv("PATH");
    if (argc != 2)
    {
        printf("Usage: %s <input file>\n", argv[0]);
        return -1;
    }
    int infile = open(argv[1], O_RDONLY);
    if (infile < 0)
    {
        perror("Error opening input file");
        return -2;
    }
    char buffer[1024];
    int readlen;
    
    // Read file line by line
    while( 1 ) {
        // Load the next line
        readlen = read_line(infile, buffer, 1024);
        if (readlen < 0)
        {
            perror("Error reading input file");
            return -3;
        }
        if (readlen == 0)
        {
            break;
        }
        int numtokens = 0;
        token_t** tokens = tokenize(buffer, readlen, &numtokens);
        assert(numtokens > 0);

        char* args[numtokens + 1];
        int index_redirection = -1;
        int index_pipe = -1;

        for (int ii = 0; ii < numtokens; ii++)
        {
            if (tokens[ii]->type == TOKEN_REDIR)
            {
                index_redirection = ii;
            }
            if (tokens[ii]->type == TOKEN_PIPE)
            {
                index_pipe = ii;
            }
            args[ii] = tokens[ii]->value;
        }

        if (index_redirection != -1)
        {
            args[index_redirection] = NULL;
        }
        else if (index_pipe != -1)
        {
            args[index_pipe] = NULL;
        }

        args[numtokens] = NULL;


        if (args[0][0] != '/')
        {
            normalize_executable(args);
        }

        if (fork() != 0)
        {
            wait(NULL);
        }
        else
        {
            if (index_pipe != -1)
            {
                int pipe_fd[2];

                if (pipe(pipe_fd) == -1)
                {
                    perror("Error creating pipe");
                    return -4;
                }

                if (fork() != 0) { // Parent Process, runs after child finishes
                    wait(NULL);
                    dup2(pipe_fd[0], STDIN_FILENO);
                    close(pipe_fd[1]);

                    char* piped_args[numtokens - index_pipe];

                    for (int ii = index_pipe + 1; ii < numtokens; ii++)
                    {
                        if (tokens[ii] == NULL) {
                            piped_args[ii - index_pipe - 1] = NULL;
                            break;
                        }
                        if (tokens[ii]->type == TOKEN_REDIR)
                        {
                            index_redirection = ii;
                        }
                        piped_args[ii - index_pipe - 1] = tokens[ii]->value;
                    }

                    piped_args[numtokens - index_pipe - 1] = NULL;

                    if (index_redirection != -1)
                    {
                        int fd = open(tokens[index_redirection + 1]->value, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if (fd < 0)
                        {
                            perror("Error opening file");
                            return -6;
                        }
                        dup2(fd, STDOUT_FILENO);
                        close(fd);
                    }

                    if (piped_args[0][0] != '/')
                    {
                        normalize_executable(piped_args);
                    }

                    execve(piped_args[0], piped_args, NULL);
                    perror("Error executing second command");
                    return -7;
                }
                else {
                    // First child process
                    dup2(pipe_fd[1], STDOUT_FILENO);
                    close(pipe_fd[0]);
                    execve(args[0], args, NULL);
                    perror("Error executing first command");
                    return -5;
                }
            }

            if (index_redirection != -1)
            {
                int fd = open(tokens[index_redirection + 1]->value, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0)
                {
                    perror("Error opening file");
                    return -4;
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            execve(args[0], args, NULL);
            perror("Error executing command");
            return -5;
        }


        // * Handle pipes
        // * Handle variable assignments
        // TODO

        // Free Variables
        for (int x = 0; x < variable_count; x++) {
            free(variables[x]->name);
            free(variables[x]->value);
            free(variables[x]);
        }
        free(variables);
        // Free tokens vector
        for (int ii = 0; ii < numtokens; ii++) {
            free(tokens[ii]->value);
            free(tokens[ii]);
        }
        free(tokens);
    }
    close(infile);
    return 0;
}