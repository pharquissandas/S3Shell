#include "s3.h"

///Simple for now, but will be expanded in a following section
void construct_shell_prompt(char shell_prompt[])
{
    strcpy(shell_prompt, "[s3]$ ");
}

///Prints a shell prompt and reads input from the user
void read_command_line(char line[])
{
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt);
    printf("%s", shell_prompt);

    ///See man page of fgets(...)
    if (fgets(line, MAX_LINE, stdin) == NULL)
    {
        perror("fgets failed");
        exit(1);
    }
    ///Remove newline (enter)
    line[strlen(line) - 1] = '\0';
}

void parse_command(char line[], char *args[], int *argsc)
{
    ///Implements simple tokenization (space delimited)
    ///Note: strtok puts '\0' (null) characters within the existing storage, 
    ///to split it into logical cstrings.
    ///There is no dynamic allocation.

    ///See the man page of strtok(...)
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1)
    {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    
    args[*argsc] = NULL; ///args must be null terminated
}

///Launch related functions
void child(char *args[], int argsc)
{
    // Attempt to replace the process image with the given command
    execvp(args[ARG_PROGNAME], args);

    // If execvp returns, it failed
    perror("execvp failed");
    exit(1);  // Exit child if execvp fails
}

void launch_program(char *args[], int argsc)
{
    // Handle built-in "exit" command before forking
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0)
    {
        printf("Exiting shell...\n");
        exit(0);
    }

    pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0)
    {
        // Child process
        child(args, argsc);
    }
    else
    {
        // Parent process — wait for child to complete
        waitpid(pid, NULL, 0);
    }
}

void launch_program_with_redirection(char *args[], int argsc){
    
    char filename[MAX_PROMPT_LEN];
    int output_mode; //0 for overwrite, 1 for append
    
    // Handle built-in "exit" command before forking
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0){
        printf("Exiting shell...\n");
        exit(0);
    }

    pid_t pid = fork();

    if (pid < 0){
        perror("fork failed");
        exit(1);

    } else if (pid == 0){
        // Child process
        for(int i=0; i<argsc; i++){
            if(strcmp(args[i], "<") == 0){ // Input redirection
                strcpy(filename, args[i+1]);
                args[i] = NULL;
                child_with_input_redirected(args, argsc, filename);
                break;

            } else if(strcmp(args[i], ">") == 0){ // Output redirection
                strcpy(filename, args[i+1]);
                args[i] = NULL;
                output_mode = 0;
                child_with_output_redirected(args, argsc, filename, output_mode);
                break;

            } else if(strcmp(args[i], ">>") == 0){ // Append redirection
                strcpy(filename, args[i+1]);
                output_mode = 1;
                args[i] = NULL;
                child_with_output_redirected(args, argsc, filename, output_mode);
                break;
            }
        }

    } else {
        // Parent process — wait for child to complete
        waitpid(pid, NULL, 0);
    }
}

void child_with_output_redirected(char *args[], int argsc, char* filename, int output_mode){
    if(output_mode == 1){ // Append mode
        int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644); // 0644 = rw-r--r--
        if (fd < 0) {
            perror("open failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);

    } else {
        int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644); // 0644 = rw-r--r--
        if (fd < 0) {
            perror("open failed");
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1); // execvp failed
}

void child_with_input_redirected(char *args[], int argsc, char* filename){
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
            perror("open failed");
            exit(1);
        }
    dup2(fd, STDIN_FILENO);
    close(fd);

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1); // execvp failed
}

int command_with_redirection(char line[]){
    // Check for < or >
    for(int i=0; line[i] != '\0'; i++){
        if(line[i] == '<' || line[i] == '>'){
            return 1; // Redirection found
        }
    }
    return 0; // No redirection found
}
