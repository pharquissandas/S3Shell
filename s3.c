#include "s3.h"

/* build the shell prompt showing the current directory */
void construct_shell_prompt(char shell_prompt[], char lwd[]) {
    char cwd[MAX_PROMPT_LEN - 10];  // leave room for formatting
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        exit(1);
    }

    snprintf(shell_prompt, MAX_PROMPT_LEN, "[%s s3]$ ", cwd);
}

/* print the prompt and read user input */
void read_command_line(char line[], char lwd[]) {
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt, lwd);
    printf("%s", shell_prompt);

    if (fgets(line, MAX_LINE, stdin) == NULL) {
        perror("fgets failed");
        exit(1);
    }

    /* remove newline character */
    line[strcspn(line, "\n")] = '\0';
}

/* tokenize command line into args array */
void parse_command(char line[], char *args[], int *argsc) {
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1) {
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    args[*argsc] = NULL;
}

/* child executes the program */
void child(char *args[], int argsc) {
    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

/* run a command normally */
void launch_program(char *args[], int argsc) {
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0) {
        printf("exiting shell...\n");
        exit(0);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        child(args, argsc);
    } else {
        waitpid(pid, NULL, 0);
    }
}

/* handle commands with redirection */
void launch_program_with_redirection(char *args[], int argsc) {
    char filename[MAX_PROMPT_LEN];
    int output_mode;

    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0) {
        printf("exiting shell...\n");
        exit(0);
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        for (int i = 0; i < argsc; i++) {
            if (strcmp(args[i], "<") == 0) {
                strcpy(filename, args[i + 1]);
                args[i] = NULL;
                child_with_input_redirected(args, argsc, filename);
                break;
            } else if (strcmp(args[i], ">") == 0) {
                strcpy(filename, args[i + 1]);
                args[i] = NULL;
                output_mode = 0;
                child_with_output_redirected(args, argsc, filename, output_mode);
                break;
            } else if (strcmp(args[i], ">>") == 0) {
                strcpy(filename, args[i + 1]);
                args[i] = NULL;
                output_mode = 1;
                child_with_output_redirected(args, argsc, filename, output_mode);
                break;
            }
        }
    } else {
        waitpid(pid, NULL, 0);
    }
}

/* redirect stdout */
void child_with_output_redirected(char *args[], int argsc, char *filename, int output_mode) {
    int fd;
    if (output_mode == 1)
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    else
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (fd < 0) {
        perror("open failed");
        exit(1);
    }

    dup2(fd, STDOUT_FILENO);
    close(fd);

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

/* redirect stdin */
void child_with_input_redirected(char *args[], int argsc, char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open failed");
        exit(1);
    }

    dup2(fd, STDIN_FILENO);
    close(fd);

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

/* check if command line has < or > */
int command_with_redirection(char line[]) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '<' || line[i] == '>')
            return 1;
    }
    return 0;
}

/* initialize last working directory */
void init_lwd(char lwd[]) {
    if (getcwd(lwd, MAX_PROMPT_LEN - 6) == NULL) {
        perror("getcwd failed");
        exit(1);
    }
}

/* check if command is cd */
int is_cd(char line[]) {
    char temp[MAX_LINE];
    strcpy(temp, line);
    char *token = strtok(temp, " ");
    return (token != NULL && strcmp(token, "cd") == 0);
}

/* handle cd logic: cd, cd -, cd <dir> */
void run_cd(char *args[], int argsc, char lwd[]) {
    char cwd[MAX_PROMPT_LEN];
    char prev[MAX_PROMPT_LEN];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        return;
    }

    strcpy(prev, cwd);

    if (argsc == 1) {
        char *home = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr, "cd: home not set\n");
            return;
        }
        if (chdir(home) != 0)
            perror("cd failed");
    } else if (strcmp(args[1], "-") == 0) {
        if (chdir(lwd) != 0)
            perror("cd failed");
        else
            printf("%s\n", lwd);
    } else {
        if (chdir(args[1]) != 0)
            perror("cd failed");
    }

    strcpy(lwd, prev);
}
