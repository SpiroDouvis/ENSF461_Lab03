#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include "parser.h"

const char* path;

int read_line(int infile, char *buffer, int maxlen)
{
    static int file_offset = 0;
    int readlen = 0;

    // TODO: Read a single line from file; retains final '\n'

    char ch;
    while (readlen < maxlen - 1) {
        int result = pread(infile, &ch, 1, file_offset++);
        if (result == 0) { // End of file
            break;
        } else if (result < 0) { // Error
            return -1;
        }
        buffer[readlen++] = ch;
        if (ch == '\n') { // End of line
            break;
        }
    }
    buffer[readlen] = '\0'; // Null-terminate the string
    
    return readlen;
}


int normalize_executable(char **command) {
    // Convert command to absolute path if needed (e.g., "ls" -> "/bin/ls")
    // Returns TRUE if command was found, FALSE otherwise
    char* pathcpy = strdup(path);
    char* tokens = strtok(pathcpy, ":");

    while(tokens != NULL) {
        // Check if command exists in the current path
        char* command_path = malloc(strlen(tokens) + strlen(command[0]) + 2);
        strcpy(command_path, tokens);
        strcat(command_path, "/");
        strcat(command_path, command[0]);
        if (access(command_path, F_OK) == 0) {
            command[0] = command_path;
            // Free the remaining tokens if necessary
            while ((tokens = strtok(NULL, ":")) != NULL) {
                // No need to process further tokens
            }
            return TRUE;
        }
        free(command_path); // Free the allocated memory
        tokens = strtok(NULL, ":");
    }

    return FALSE;
}


void update_variable(char* name, char* value) {
    // Update or create a variable
}

char* lookup_variable(char* name) {
    // Lookup a variable
    return NULL;
}


int main(int argc, char *argv[]) {    

    path = getenv("PATH");

    if(argc != 2) {
        printf("Usage: %s <input file>\n", argv[0]);
        return -1;
    }

    int infile = open(argv[1], O_RDONLY);
    if(infile < 0) {
        perror("Error opening input file");
        return -2;
    }

    char buffer[1024];
    int readlen;
    
    // Read file line by line
    while( 1 ) {

        // Load the next line
        readlen = read_line(infile, buffer, 1024);
        if(readlen < 0) {
            perror("Error reading input file");
            return -3;
        }
        if(readlen == 0) {
            break;
        }

        // Tokenize the line
        int numtokens = 0;
        token_t** tokens = tokenize(buffer, readlen, &numtokens);
        assert(numtokens > 0);

        // Parse token list
        // * Organize tokens into command parameters
        char* args[numtokens+1];
            for (int ii = 0; ii < numtokens; ii++) {
                args[ii] = tokens[ii]->value;
            }
        args[numtokens] = NULL;


        // * Check if command is a variable assignment
        // * Check if command has a redirection
        // * Expand variables if any
        // * Normalize executables
        if (args[0][0] != '/'){
            normalize_executable(args);
        }
        // * Check if pipes are present

        // * Check if pipes are present
        // TODO

        // Run commands
        if (fork()!=0){
            //in parent
        } else{
            //in child

            execve(args[0], args, NULL);

        }
        // * Fork and execute commands
        // * Handle pipes
        // * Handle redirections
        // * Handle pipes
        // * Handle variable assignments
        // TODO

        // Free tokens vector
        for (int ii = 0; ii < numtokens; ii++) {
            free(tokens[ii]->value);
            free(tokens[ii]);
        }
        free(tokens);
    }

    close(infile);
    
    // Remember to deallocate anything left which was allocated dynamically
    // (i.e., using malloc, realloc, strdup, etc.)

    return 0;
}


