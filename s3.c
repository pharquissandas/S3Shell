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
//========================================================================

void execute_command(char *cmd, char lwd[]) {
    char cmd_copy[MAX_LINE];
    char *args[MAX_ARGS];
    int argsc;

    // make a copy to avoid modifying original
    strcpy(cmd_copy, cmd);

    if (command_with_subshell(cmd_copy)) {
        launch_subshell(cmd_copy);
    } else if (is_cd(cmd_copy)) {
        parse_command(cmd_copy, args, &argsc);
        run_cd(args, argsc, lwd);
    } else if (command_with_pipe(cmd_copy)) {
        char *pipe_commands[MAX_ARGS];
        int num_cmds = split_pipeline(cmd_copy, pipe_commands);
        launch_pipeline(pipe_commands, num_cmds);
    } else if (command_with_redirection(cmd_copy)) {
        parse_command(cmd_copy, args, &argsc);
        launch_program_with_redirection(args, argsc);
    } else {
        parse_command(cmd_copy, args, &argsc);
        launch_program(args, argsc);
    }
}

//========================================================================

/* check if command line has < or > */
int command_with_redirection(char line[]) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '<' || line[i] == '>')
            return 1;
    }
    return 0;
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
    if (output_mode == 1){
        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } else {
        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }
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

//========================================================================

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

//========================================================================

/* detects pipes */
int command_with_pipe(char line[]) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '|')
            return 1;
    }
    return 0;
}

/* split into separate commands */
int split_pipeline(char line[], char *commands[]){
    int count = 0;
    char *token = strtok(line, "|");
    while (token != NULL && count < MAX_ARGS-1){
        commands[count++] = token;
        token = strtok(NULL, "|");
    }
    commands[count] = NULL;
    return count;
}

/* launch the full pipeline */
void launch_pipeline(char *commands[], int num_cmds){
    int pipefds[2 * (num_cmds-1)];
    pid_t pids[num_cmds];

    // create all pipes
    for (int i = 0; i < num_cmds - 1; i++){
        if (pipe(pipefds + i * 2) < 0){
            perror("pipe failed");
            exit(1);
        }
    }

    // loop through all commands in the pipeline
    for (int i = 0; i < num_cmds; i++){
        pids[i] = fork();

        if (pids[i] < 0) {
            perror("fork failed");
            exit(1);
        }

        if (pids[i] == 0) { // child
            // if not the first command, redirect stdin to previous pipe
            if (i > 0){
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }

            // if not the last command, redirect stdout to next pipe
            if (i < num_cmds - 1){
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            // close all pipe fds in the child
            for (int j = 0; j < 2 * (num_cmds - 1); j++){
                close(pipefds[j]);
            }

            // parse current command
            char *args[MAX_ARGS];
            int argsc;
            parse_command(commands[i], args, &argsc);

            // handle redirection (only applies if '>' or '>>' appear in this command)
            for (int j = 0; args[j] != NULL; j++){
                if (strcmp(args[j], ">") == 0 || strcmp(args[j], ">>") == 0){
                    int append = (strcmp(args[j], ">>") == 0);
                    char *filename = args[j+1];

                    int fd;
                    if (append){
                        fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    } else {
                        fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    }
                    if (fd < 0){
                        perror("open failed");
                        exit(1);
                    }

                    dup2(fd, STDOUT_FILENO); // redirect stdout to file
                    close(fd);

                    args[j] = NULL; // end args before '>'
                    break;
                }
            }

            // run the command
            execvp(args[ARG_PROGNAME], args);
            perror("execvp failed");
            exit(1);
        }
    }

    // parent closes all pipe fds
    for (int i = 0; i < 2 * (num_cmds - 1); i++){
        close(pipefds[i]);
    }

    // wait for all children
    for (int i = 0; i < num_cmds; i++){
        waitpid(pids[i], NULL, 0);
    }
}

//========================================================================

// check for batched commands separated by ; only in the top level
int command_with_batch(char line[]) {
    int level = 0;
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '('){
            level++;
        } else if (line[i] == ')'){
            level--;
        } else if (line[i] == ';' && level == 0){
            return 1;
        }
    }
    return 0;
}

int split_batch(char line[], char *commands[]) {
    int count = 0;
    int level = 0;
    char *start = line;

    while (*start == ' ' || *start == '\t'){ // skip whitespace
        start++;
    } 

    for (char *p = line; *p != '\0'; *p++){
        if (*p == '('){
            level++;
        } else if (*p == ')'){
            level--;
        } else if (*p == ';' && level == 0){
            *p = '\0';
      
            char *end = p - 1;
            while (end > start && (*end == ' ' || *end == '\t')) {// trim whitespace
                *end = '\0';
                end--;
            }
            commands[count++] = start;
            start = p + 1;

            while (*start == ' ' || *start == '\t'){ // skip whitespace
                start++;
            }
        }
    }

    // add last command
    if (*start != '\0' && count < MAX_ARGS - 1) {
        
        char *end = start + strlen(start) - 1;
        while(end > start && (*end == ' ' || *end == '\t')) { // trim whitespace
            *end = '\0';
            end--;
        }
        commands[count++] = start;
    }

    commands[count] = NULL;
    return count;
}

//========================================================================

int command_with_subshell(char line[]) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '(' || line[i] == ')')
            return 1;
    }
    return 0;
}

void split_subshell(char *line, char *subcmd) {
    char *start = strchr(line, '(');
    char *end   = strchr(line, ')');

    if (!start || !end || end < start) {
        fprintf(stderr, "mismatched parentheses");
        exit(1);
    }

    // copy text between '(' and ')'
    strncpy(subcmd, start+1, end - start - 1);
    subcmd[end - start - 1] = '\0';
}

void launch_subshell(char *line) {

    char subcmd[MAX_LINE];
    split_subshell(line, subcmd);

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) { // execute command exactly like main but in child
        
        char subshell_lwd[MAX_PROMPT_LEN - 6];
        init_lwd(subshell_lwd);

        char subcopy[MAX_LINE];
        strcpy(subcopy, subcmd);

        if (command_with_batch(subcopy)) {
            char *batch[MAX_ARGS];
            int batch_count = split_batch(subcopy, batch);

            for (int i = 0; i < batch_count; i++) {
                execute_command(batch[i], subshell_lwd);
            }
        } else {
            execute_command(subcopy, subshell_lwd);
        }

        exit(0);
    }

    waitpid(pid, NULL, 0);
}
