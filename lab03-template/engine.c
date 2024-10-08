#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include "parser.h"

const char *path;

int read_line(int infile, char *buffer, int maxlen)
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

int normalize_executable(char **command)
{
    char *pathcpy = strdup(path);
    char *tokens = strtok(pathcpy, ":");
    while (tokens != NULL)
    {
        char *command_path = malloc(strlen(tokens) + strlen(command[0]) + 2);
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

void update_variable(char *name, char *value)
{
    // Update or create a variable
}

char *lookup_variable(char *name)
{
    // Lookup a variable
    return NULL;
}

int main(int argc, char *argv[])
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

    while (1)
    {
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
        token_t **tokens = tokenize(buffer, readlen, &numtokens);
        assert(numtokens > 0);

        char *args[numtokens + 1];
        int index_redirection = -1;
        int index_pipe = -1;

        for (int ii = 0; ii < numtokens; ii++)
        {
            if (tokens[ii]->type == TOKEN_REDIR)
            {
                index_redirection = ii;
                break;
            }
            if (tokens[ii]->type == TOKEN_PIPE)
            {
                index_pipe = ii;
                break;
            }
            args[ii] = tokens[ii]->value;
        }

        if (index_redirection != -1)
        {
            args[index_redirection] = NULL;
        }
        else
        {
            args[numtokens] = NULL;
        }

        if (args[0][0] != '/')
        {
            normalize_executable(args);
        }

        if (index_pipe != -1)
        {
            int pipe_fd[2];

            if (pipe(pipe_fd) == -1)
            {
                perror("Error creating pipe");
                return -4;
            }

            if (fork() == 0)
            {
                // First child process
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
                execve(args[0], args, NULL);
                perror("Error executing first command");
                return -5;
            }

            if (fork() == 0)
            {
                // Second child process
                close(pipe_fd[1]);
                dup2(pipe_fd[0], STDIN_FILENO);
                close(pipe_fd[0]);

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

                execve(args[index_pipe + 1], &args[index_pipe + 1], NULL);
                perror("Error executing second command");
                return -7;
            }

            close(pipe_fd[0]);
            close(pipe_fd[1]);
            wait(NULL);
            wait(NULL);
        }
        else
        {
            if (fork() != 0)
            {
                wait(NULL);
            }
            else
            {
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
        }

        for (int ii = 0; ii < numtokens; ii++)
        {
            free(tokens[ii]->value);
            free(tokens[ii]);
        }
        free(tokens);
    }
    close(infile);
    return 0;
}