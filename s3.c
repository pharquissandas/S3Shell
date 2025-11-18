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

/* handle commands with redirection */
void launch_program_with_redirection(char *args[], int argsc){
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0){
        exit(0);
    }
    pid_t pid = fork();
    if (pid < 0){ 
        perror("fork failed"); exit(1);
    }
    else if (pid == 0){
        child_with_redirection(args, argsc);
    }
    else{
        waitpid(pid, NULL, 0);
    }
}

void child_with_redirection(char *args[], int argsc){
    int input_fd = -1, output_fd = -1, output_mode = 0; /* 0=truncate,1=append */
    char *input_file = NULL, *output_file = NULL;

    /* scan args for redirection operators */
    for (int i = 0; i < argsc; i++){
        if (strcmp(args[i], "<") == 0){
            if (i+1 < argsc){
                input_file = args[i + 1];
            }
            args[i] = NULL; /* terminate args before < */
            break;
        }
        else if (strcmp(args[i], ">>") == 0){
            if (i+1 < argsc){
                output_file = args[i + 1];
            }
            output_mode = 1;
            args[i] = NULL;
            break;
        }
        else if (strcmp(args[i], ">") == 0){
            if (i+1 < argsc){
                output_file = args[i + 1];
            }
            output_mode = 0;
            args[i] = NULL;
            break;
        }
    }

    /* handle input redirection */
    if (input_file){
        input_fd = open(input_file, O_RDONLY);
        if (input_fd < 0){ 
            perror("open input failed"); 
            exit(1); 
        }
        if (dup2(input_fd, STDIN_FILENO) < 0){ 
            perror("dup2 input failed"); 
            exit(1); 
        }
        close(input_fd);
    }

    /* handle output redirection */
    if (output_file){
        if (output_mode == 1){
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        } 
        else{
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (output_fd < 0){ 
            perror("open output failed"); 
            exit(1); 
        }
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);
    }

    if (args[ARG_PROGNAME] == NULL){
        /* nothing to exec (this can happen if redirection consumed tokens) */
        exit(0);
    }

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
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

/* trim leading & trailing whitespace (in-place) */
static void trim_ws(char *s){
    char *end;
    while(isspace((unsigned char)*s)) s++;
    if(*s == 0){ 
        *s = 0; 
        return; 
    }
    end = s + strlen(s) - 1;
    while(end > s && isspace((unsigned char)*end)){
        end--;
    }
    *(end+1) = '\0';
}

/* split into separate commands in the topmost layer */
int split_pipeline(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;

    while(*start==' '||*start=='\t'){
        start++; /* skip leading space */
    }

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

/* helper to extract subshell (first top-level pair) -- returns 0 on success */
void extract_next_subshell(char *line, int start, int *sub_start, int *sub_end){
    int level=0;
    *sub_start = *sub_end = -1;
    for(int i=start; line[i]!='\0'; i++){
        if(line[i]=='('){
            if(level==0) *sub_start = i;
            level++;
        } else if(line[i]==')'){
            level--;
            if(level==0){
                *sub_end = i;
                return;
            }
        }
    }
}

/* launch the full pipeline */
void launch_pipeline(char *commands[], int num_cmds){
    int pipefds[2*(num_cmds-1)];
    pid_t pids[num_cmds];

    /* create all pipes */
    for (int i=0;i<num_cmds-1;i++)
        if (pipe(pipefds + i*2) < 0){ 
            perror("pipe failed"); exit(1); 
        }

    for (int i=0;i<num_cmds;i++){
        pids[i] = fork();
        if (pids[i]<0){ 
            perror("fork failed"); 
            exit(1); 
        }

        if (pids[i]==0){
            /* child */
            /* input from previous pipe */
            if(i>0){
                if (dup2(pipefds[(i-1)*2], STDIN_FILENO) < 0){ 
                    perror("dup2 failed"); 
                    exit(1); 
                }
            }
            /* output to next pipe */
            if(i<num_cmds-1){
                if (dup2(pipefds[i*2+1], STDOUT_FILENO) < 0){ 
                    perror("dup2 failed"); 
                    exit(1); 
                }
            }

            /* close all pipes in child */
            for(int j=0;j<2*(num_cmds-1);j++){
                close(pipefds[j]);
            }

            /* handle if this stage is a subshell (starts with '(') */
            char stage[MAX_LINE];
            strncpy(stage, commands[i], MAX_LINE);
            trim_ws(stage);
            if (stage[0] == '('){
                int sstart, send;
                extract_next_subshell(stage, 0, &sstart, &send);
                if (sstart != -1 && send != -1){
                    char inner[MAX_LINE];
                    int len = send - sstart - 1;
                    if (len >= MAX_LINE) len = MAX_LINE-1;
                    strncpy(inner, stage + sstart + 1, len);
                    inner[len] = '\0';

                    /* execute the subshell content inside this child */
                    char sub_lwd[MAX_PROMPT_LEN-6];
                    init_lwd(sub_lwd);

                    /* run the inner commands: it may be a batch, pipeline, etc.
                       execute_command will fork as needed. */
                    execute_command(inner, sub_lwd);
                    /* when inner returns, exit child */
                    exit(0);
                }
            }

            /* regular stage: parse and handle redirection or exec */
            char temp[MAX_LINE];
            strncpy(temp, commands[i], MAX_LINE);
            char *args[MAX_ARGS]; int argsc;
            parse_command(temp, args, &argsc);

            if (argsc == 0){
                exit(0);
            }

            if (command_with_redirection(commands[i])){
                /* reuse child_with_redirection which handles <,>,>> */
                child_with_redirection(args, argsc);
                exit(1);
            }

            execvp(args[ARG_PROGNAME], args);
            perror("execvp failed");
            exit(1);
        }
    }

    /* parent closes all pipes */
    for(int i=0;i<2*(num_cmds-1);i++){
        close(pipefds[i]);
    }

    /* wait for all children */
    for(int i=0;i<num_cmds;i++){
        waitpid(pids[i], NULL, 0);
    }
}

//========================================___BATCH___========================================

/* check for batched commands separated by ; in the topmost level */
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

/* splits batched commands in the topmost layer*/
int split_batch(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;

    while(*start==' '||*start=='\t'){
        start++; /* skip leading space */
    }

    for(char *p=line;*p!='\0';p++){
        if(*p=='('){
            p++;
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
    } /* add last command */
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

/* launch subshell, supports nested commands and outer redirection */
void launch_subshell(char *line){
    /* line may contain outer redirection after the closing parenthesis */
    int sstart = -1, send = -1;
    extract_next_subshell(line, 0, &sstart, &send);
    if (sstart == -1 || send == -1){
        fprintf(stderr, "No subshell found\n");
        return;
    }

    /* extract inner command */
    char inner[MAX_LINE];
    int inner_len = send - sstart - 1;
    if (inner_len >= MAX_LINE){
        inner_len = MAX_LINE-1;
    }
    strncpy(inner, line + sstart + 1, inner_len);
    inner[inner_len] = '\0';

    /* figure out outer tokens (after closing paren) */
    char after[MAX_LINE] = {0};
    if (send + 1 < (int)strlen(line)){
        strncpy(after, line + send + 1, MAX_LINE-1);
        trim_ws(after);
    }

    /* if there is an outer redirection on the top level, parse it and apply to the subshell process */
    int has_outer_redir = command_with_redirection(after);

    pid_t pid = fork();
    if (pid < 0){
        perror("fork failed"); 
        return; 
    }
    if (pid == 0){
        /* child: runs the subshell content */
        char sub_lwd[MAX_PROMPT_LEN-6];
        init_lwd(sub_lwd);

        if (has_outer_redir){
            /* parse after to find <, >, >> and apply them */
            char tmp[MAX_LINE];
            strncpy(tmp, after, MAX_LINE);
            char *tokens[MAX_ARGS];
            int tac = 0;
            char *tok = strtok(tmp, " ");
            while(tok && tac < MAX_ARGS-1){
                tokens[tac++] = tok, tok = strtok(NULL, " ");
            }
            tokens[tac] = NULL;

            /* find first redirection operator in tokens */
            for (int i = 0; i < tac; i++){
                if (strcmp(tokens[i], "<") == 0 && i+1 < tac){
                    int fd = open(tokens[i+1], O_RDONLY);
                    if (fd < 0){ 
                        perror("open input failed"); 
                        exit(1); 
                    }
                    if (dup2(fd, STDIN_FILENO) < 0){ 
                        perror("dup2 failed"); 
                        exit(1); 
                    }
                    close(fd);
                } else if (strcmp(tokens[i], ">") == 0 && i+1 < tac){
                    int fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0){ 
                        perror("open output failed"); 
                        exit(1); 
                    }
                    if (dup2(fd, STDOUT_FILENO) < 0){ 
                        perror("dup2 failed"); 
                        exit(1); 
                    }
                    close(fd);
                } else if (strcmp(tokens[i], ">>") == 0 && i+1 < tac){
                    int fd = open(tokens[i+1], O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0){ 
                        perror("open output failed"); 
                        exit(1); 
                    }
                    if (dup2(fd, STDOUT_FILENO) < 0){ 
                        perror("dup2 failed"); 
                        exit(1); 
                    }
                    close(fd);
                }
            }
        }

        /* now run inner content (it may be a batch or pipeline or simple command) */
        if (command_with_batch(inner)){
            char *batch[MAX_ARGS];
            int batch_count = split_batch(inner, batch);
            for (int i = 0; i < batch_count; i++){
                execute_command(batch[i], sub_lwd);
            }
        } else {
            execute_command(inner, sub_lwd);
        }
        exit(0);
    }
    waitpid(pid, NULL, 0);
}

//========================================___EXECUTE COMMAND___========================================

/* executes command */
void execute_command(char *cmd, char lwd[]){
    /* trim leading/trailing whitespace in copy */
    char copy[MAX_LINE];
    strncpy(copy, cmd, MAX_LINE-1);
    copy[MAX_LINE-1] = '\0';
    trim_ws(copy);

    if (copy[0] == '\0') return;

    /* 1) batched commands at top level */
    if (command_with_batch(copy)){
        char *batch[MAX_ARGS];
        int batch_count = split_batch(copy, batch);
        for (int i = 0; i < batch_count; i++){
            execute_command(batch[i], lwd);
        }
        return;
    }

    /* 2) if entire command is a top-level subshell OR contains a subshell at top-level */
    if (command_with_subshell(copy)){
        /* if it's a pipeline where a stage is a subshell, the pipeline handler will deal with it.
           otherwise, it's a subshell possibly with outer redirection; launch_subshell handles both. */
        /* check if this line is a pipeline at top level */
        if (command_with_pipe(copy)){
            char *pipe_cmds[MAX_ARGS];
            int num_cmds = split_pipeline(copy, pipe_cmds);
            launch_pipeline(pipe_cmds, num_cmds);
            return;
        }

        /* otherwise launch subshell (handles outer redirection) */
        launch_subshell(copy);
        return;
    }

    /* 3) cd built-in */
    if (is_cd(copy)){
        char *args[MAX_ARGS];
        int argsc;
        parse_command(copy, args, &argsc);
        run_cd(args, argsc, lwd);
        return;
    }

    /* 4) pipeline */
    if (command_with_pipe(copy)){
        char *pipe_cmds[MAX_ARGS];
        int num_cmds = split_pipeline(copy, pipe_cmds);
        launch_pipeline(pipe_cmds, num_cmds);
        return;
    }

    /* 5) redirection only */
    if (command_with_redirection(copy)){
        char *args[MAX_ARGS];
        int argsc;
        parse_command(copy, args, &argsc);
        launch_program_with_redirection(args, argsc);
        return;
    }

    /* 6) plain command */
    {
        char *args[MAX_ARGS];
        int argsc;
        parse_command(copy, args, &argsc);
        if (argsc == 0){
            return;
        }
        launch_program(args, argsc);
    }
}