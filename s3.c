#include "s3.h"
#include <ctype.h>
#include <errno.h>

/* build the shell prompt showing the current directory */
void construct_shell_prompt(char shell_prompt[], char lwd[]){
    char cwd[MAX_PROMPT_LEN - 10]; /* leave room for formatting */
    if (getcwd(cwd, sizeof(cwd)) == NULL){
        perror("getcwd failed");
        exit(1);
    }
    snprintf(shell_prompt, MAX_PROMPT_LEN, "[%s s3]$ ", cwd);
}

/* print the prompt and read user input */
void read_command_line(char line[], char lwd[]){
    char shell_prompt[MAX_PROMPT_LEN];
    construct_shell_prompt(shell_prompt, lwd);
    printf("%s", shell_prompt);
    fflush(stdout);

    if (fgets(line, MAX_LINE, stdin) == NULL){
        if (feof(stdin)) exit(0);
        perror("fgets failed");
        exit(1);
    }
    line[strcspn(line, "\n")] = '\0'; /* remove newline */
}

/* tokenize command line into args array */
void parse_command(char line[], char *args[], int *argsc){
    char *token = strtok(line, " ");
    *argsc = 0;
    while (token != NULL && *argsc < MAX_ARGS - 1){
        args[(*argsc)++] = token;
        token = strtok(NULL, " ");
    }
    args[*argsc] = NULL;
}

/* child executes the program */
void child(char *args[], int argsc){
    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

/* run a command normally */
void launch_program(char *args[], int argsc){
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0){
        /* shell should exit (parent) */
        exit(0);
    }

    pid_t pid = fork();
    if (pid < 0){
        perror("fork failed");
        exit(1);
    } else if (pid == 0){
        child(args, argsc);
    } else{
        waitpid(pid, NULL, 0);
    }
}

//========================================___CD___========================================

/* initialize last working directory */
void init_lwd(char lwd[]){
    if (getcwd(lwd, MAX_PROMPT_LEN - 6) == NULL){
        perror("getcwd failed");
        exit(1);
    }
}

/* check if command is cd */
int is_cd(char line[]){
    char temp[MAX_LINE];
    strcpy(temp, line);
    char *token = strtok(temp, " ");
    return (token != NULL && strcmp(token, "cd") == 0);
}

/* handle cd logic: cd, cd -, cd <dir> */
void run_cd(char *args[], int argsc, char lwd[]){
    char cwd[MAX_PROMPT_LEN];
    char prev[MAX_PROMPT_LEN];

    if (getcwd(cwd, sizeof(cwd)) == NULL){
        perror("getcwd failed");
        return;
    }
    strcpy(prev, cwd);

    if (argsc == 1){
        char *home = getenv("HOME");
        if (home == NULL){
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        if (chdir(home) != 0){
            perror("cd failed");
        }
    } else if (strcmp(args[1], "-") == 0){
        if (chdir(lwd) != 0){
            perror("cd failed");
        }
        else printf("%s\n", lwd);
    } else{
        if (chdir(args[1]) != 0){
            perror("cd failed");
        }
    }

    strcpy(lwd, prev);
}

//========================================___REDIRECTION___========================================

/* check if command line has < or > in the topmost level */
int command_with_redirection(char line[]){
    int level = 0;
    for(int i=0; line[i]!='\0'; i++){
        if(line[i]=='('){
            level++;
        }
        else if(line[i]==')'){
            level--;
        }
        else if((line[i]=='<' || line[i]=='>') && level==0){
            return 1;
        }
    }
    return 0;
}

// parent manages fork and wait. line passed instead of args to allow parsing only top layer
void launch_program_with_redirection(char *line, char lwd[]){

    pid_t pid = fork();
    if (pid < 0){ 
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0){
        child_with_redirection(line, lwd);
    }
    else{
        waitpid(pid, NULL, 0);
    }
}

// child handles redirection and execution
void child_with_redirection(char *line, char lwd[]){
    char command[MAX_LINE];
    strcpy(command, line);
    
    char *input_file = NULL;
    char *output_file = NULL;
    int output_mode = 0; // 0 = trunc, 1 = append
    int input_fd = -1;
    int output_fd = -1;

    // Markers to cut the string later
    char *cut_input = NULL;
    char *cut_output = NULL;

    // parse for redirection at topmost level
    int level = 0;
    for (int i = 0; command[i] != '\0'; i++) {
        if (command[i] == '(') level++;
        else if (command[i] == ')') { if (level > 0) level--; }
        else if (level == 0) {
            if (command[i] == '<') {
                cut_input = &command[i];
                input_file = &command[i+1];
            } else if (command[i] == '>') {
                cut_output = &command[i];
                if (command[i+1] == '>') {
                    output_mode = 1;
                    output_file = &command[i+2];
                    i++; 
                } else {
                    output_mode = 0;
                    output_file = &command[i+1];
                }
            }
        }
    }

    if (cut_input) {
        *cut_input = '\0';
    }
    if (cut_output) {
        *cut_output = '\0';
    }

    // trim whitespace from filenames
    if (input_file) {
        while(isspace(*input_file)) input_file++;
        char *end = input_file + strlen(input_file) - 1;
        while(end > input_file && isspace(*end)) *end-- = '\0';
    }
    if (output_file) {
        while(isspace(*output_file)) output_file++;
        char *end = output_file + strlen(output_file) - 1;
        while(end > output_file && isspace(*end)) *end-- = '\0';
    }

    // perform redirection
    if (input_file){
        input_fd = open(input_file, O_RDONLY);
        if (input_fd < 0){ 
            perror("open input failed"); 
            exit(1); 
        }
        dup2(input_fd, STDIN_FILENO);
        if (dup2(input_fd, STDIN_FILENO) < 0){ 
            perror("dup2 input failed"); 
            exit(1); 
        }
        close(input_fd);
    }

    if (output_file){
        if (output_mode){
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644); // 0644 = rw-r--r--
        }else{
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644); // 0644 = rw-r--r--
        }
        if (output_fd < 0){ 
            perror("open output failed"); 
            exit(1); 
        }
        dup2(output_fd, STDOUT_FILENO);
        if (dup2(output_fd, STDOUT_FILENO) < 0){ 
            perror("dup2 output failed"); 
            exit(1); 
        }
        close(output_fd);
    }

    // recursively execute command
    execute_command(command, lwd);
    exit(0);

}

//========================================___PIPES___========================================

/* detects pipes in topmost level */
int command_with_pipe(char line[]){
    int level = 0;
    for(int i=0; line[i]!='\0'; i++){
        if(line[i]=='('){
            level++;
        }
        else if(line[i]==')'){ 
            level--;
        }
        else if(line[i]=='|' && level==0){
            return 1;
        }
    }
    return 0;
}

/* split into separate commands in the topmost layer */
int split_pipeline(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;

    while(*start==' '||*start=='\t'){
        start++; /* skip leading whitespace */
    }
    // detect pipes at topmost level
    for(char *p=line;*p!='\0';p++){
        if(*p=='('){
            level++;
        }
        else if(*p==')'){
            level--;
        }
        else if(*p=='|' && level==0){
            *p='\0';
            char *end = p-1;
            while(end>start && isspace((unsigned char)*end)){
                *end--='\0';
            }
            commands[count++] = start;
            start=p+1;
            while(*start==' '||*start=='\t'){
                start++;
            }
        }
    } /* add last commands*/
    if(*start!='\0' && count<MAX_ARGS-1){
        char *end=start+strlen(start)-1;
        while(end>start && isspace((unsigned char)*end)) *end--='\0';
        commands[count++] = start;
    }
    commands[count]=NULL;
    return count;
}

/* launch a pipeline of commands */
void launch_pipeline(char *commands[], int num_cmds){
    int pipefds[2 * (num_cmds - 1)];
    pid_t pids[num_cmds];

    for (int i=0; i<num_cmds-1; i++){
        if (pipe(pipefds + i*2) < 0){ 
            perror("pipe failed"); 
            exit(1); 
        }
    }
    // lwd for child processes
    char child_lwd[MAX_PROMPT_LEN - 6];
    init_lwd(child_lwd);

    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0){
            perror("fork failed");
            exit(1);
        }

        if (pids[i] == 0) {
            if (i > 0){
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            if (i < num_cmds - 1){
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }
            for (int j = 0; j < 2 * (num_cmds - 1); j++){
                close(pipefds[j]);
            }
            // recursively execute command
            execute_command(commands[i], child_lwd);
            exit(0);
        }
    }

    for (int i = 0; i < 2 * (num_cmds - 1); i++){
        close(pipefds[i]);
    }
    for (int i = 0; i < num_cmds; i++){
        waitpid(pids[i], NULL, 0);
    }
}

//========================================___BATCH___========================================

// detect batch commands at topmost level
int command_with_batch(char line[]){
    int level = 0;
    for(int i=0; line[i]!='\0'; i++){
        if(line[i]=='('){
            level++;
        }
        else if(line[i]==')'){
            level--;
        }
        else if(line[i]==';' && level==0){
            return 1;
        }
    }
    return 0;
}

// split batch commands at topmost level
int split_batch(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;
    while(*start==' '||*start=='\t'){
        start++;
    }
    for(char *p=line;*p!='\0';p++){
        if(*p=='('){
            level++;
        }
        else if(*p==')'){
            level--;
        }
        else if(*p==';' && level==0){
            *p='\0';
            char *end = p-1;
            while(end>start && isspace((unsigned char)*end)){
                *end--='\0';
            }
            commands[count++] = start;
            start=p+1;
            while(*start==' '||*start=='\t'){
                start++;
            }
        }
    } // add last command
    if(*start!='\0' && count<MAX_ARGS-1){
        char *end=start+strlen(start)-1;
        while(end>start && isspace((unsigned char)*end)){
            *end--='\0';
        }
        commands[count++] = start;
    }
    commands[count]=NULL;
    return count;
}

//========================================___SUBSHELL___========================================

/* nested subshells detection */
int command_with_subshell(char *line){
    for(int i=0; line[i]!='\0'; i++)
        if(line[i]=='('){
            return 1;
        }
    return 0;
}

/* launch a subshell command */
void launch_subshell(char *line) {
    char *start = strchr(line, '(');
    char *end = strrchr(line, ')');

    if (!start || !end || end < start) {
        fprintf(stderr, "Subshell syntax error\n");
        return;
    }

    // copy text inside '(' and ')'
    char subcmd[MAX_LINE];
    int len = end - start - 1;
    strncpy(subcmd, start + 1, len);
    subcmd[len] = '\0';

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }
    // child process
    if (pid == 0) {
        char sub_lwd[MAX_PROMPT_LEN - 6]; // last working directory for subshell
        init_lwd(sub_lwd);
        // recursively execute the command inside the subshell
        execute_command(subcmd, sub_lwd);
        exit(0);
    } else { 
        waitpid(pid, NULL, 0); // parent waits for child
    }
}

//========================================___EXECUTE COMMAND___========================================

void execute_command(char *cmd, char lwd[]) {
    char copy[MAX_LINE];
    strcpy(copy, cmd);

    // batch (;)
    if (command_with_batch(copy)) {
        char *batch[MAX_ARGS];
        int count = split_batch(copy, batch);
        for (int i = 0; i < count; i++) execute_command(batch[i], lwd);
        return;
    }
    // pipe (|) 
    if (command_with_pipe(copy)) {
        char *pipe_cmds[MAX_ARGS];
        int num_cmds = split_pipeline(copy, pipe_cmds);
        launch_pipeline(pipe_cmds, num_cmds);
        return;
    }
    // redirection (<, >)
    // redirection before subshell ensures (ls) > file is treated as a redirection of a subshell
    if (command_with_redirection(copy)) {
        launch_program_with_redirection(copy, lwd); 
        return;
    }
    // subshell
    if (command_with_subshell(copy)) {
        launch_subshell(copy);
        return;
    }
    // CD
    if (is_cd(copy)) {
        char *args[MAX_ARGS];
        int argsc;
        parse_command(copy, args, &argsc);
        run_cd(args, argsc, lwd);
        return;
    }
    // simple command
    char *args[MAX_ARGS];
    int argsc;
    parse_command(copy, args, &argsc);
    if (argsc > 0) {
        launch_program(args, argsc);
    }
}
