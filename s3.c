#include "s3.h"
#include <ctype.h>


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

    if (fgets(line, MAX_LINE, stdin) == NULL){
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
        printf("exiting shell...\n");
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
        if (chdir(home) != 0) perror("cd failed");
    } else if (strcmp(args[1], "-") == 0){
        if (chdir(lwd) != 0) perror("cd failed");
        else printf("%s\n", lwd);
    } else{
        if (chdir(args[1]) != 0) perror("cd failed");
    }

    strcpy(lwd, prev);
}

/* check if command line has < or > */
int command_with_redirection(char line[]){
    for(int i = 0; line[i] != '\0'; i++){
        if (line[i] == '<' || line[i] == '>')
            return 1;
    }
    return 0;
}

//========================================___REDIRECTION___========================================

/* handle commands with redirection */
void launch_program_with_redirection(char *args[], int argsc){
    if (argsc > 0 && strcmp(args[ARG_PROGNAME], "exit") == 0){
        printf("exiting shell...\n");
        exit(0);
    }
    pid_t pid = fork();
    if (pid < 0) { perror("fork failed"); exit(1); }
    else if (pid == 0) {
        child_with_redirection(args, argsc); 
    } else {
        waitpid(pid, NULL, 0);
    }
}

void child_with_redirection(char *args[], int argsc){
    int input_fd = -1, output_fd = -1, output_mode = 0; /* 0=truncate,1=append */
    char *input_file = NULL, *output_file = NULL;

    /* scan args for redirection operators */
    for (int i = 0; i < argsc; i++){
        if (strcmp(args[i], "<") == 0){
            input_file = args[i + 1];
            args[i] = NULL; /* terminate args before < */
        }
        else if (strcmp(args[i], ">") == 0){
            output_file = args[i + 1];
            output_mode = 0;
            args[i] = NULL;
        }
        else if (strcmp(args[i], ">>") == 0){
            output_file = args[i + 1];
            output_mode = 1;
            args[i] = NULL;
        }
    }

    /* handle input redirection */
    if (input_file) {
        input_fd = open(input_file, O_RDONLY);
        if (input_fd < 0) { perror("open input failed"); exit(1); }
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);
    }

    /* handle output redirection */
    if (output_file) {
        if (output_mode == 1) {
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        } else {
            output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (output_fd < 0) { perror("open output failed"); exit(1); }
        dup2(output_fd, STDOUT_FILENO);
        close(output_fd);
    }

    execvp(args[ARG_PROGNAME], args);
    perror("execvp failed");
    exit(1);
}

//========================================___PIPES___========================================

/* detects pipes */
int command_with_pipe(char line[]){
    for (int i = 0; line[i] != '\0'; i++)
        if (line[i] == '|') return 1;
    return 0;
}

/* split into separate commands in the topmost layer */
int split_pipeline(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;

    while(*start==' '||*start=='\t') start++; /* skip leading space */

    for(char *p=line;*p!='\0';p++){
        if(*p=='(') level++;
        else if(*p==')') level--;
        else if(*p=='|' && level==0){
            *p='\0';
            char *end = p-1;
            while(end>start && isspace(*end)) *end--='\0';
            commands[count++] = start;
            start=p+1;
            while(*start==' '||*start=='\t') start++;
        }
    } /* add last commands*/
    if(*start!='\0' && count<MAX_ARGS-1){
        char *end=start+strlen(start)-1;
        while(end>start && isspace(*end)) *end--='\0';
        commands[count++] = start;
    }
    commands[count]=NULL;
    return count;
}

/* launch the full pipeline */
void launch_pipeline(char *commands[], int num_cmds){
    int pipefds[2*(num_cmds-1)];
    pid_t pids[num_cmds];

    // create all pipes
    for (int i=0;i<num_cmds-1;i++)
        if (pipe(pipefds + i*2) < 0){ perror("pipe failed"); exit(1); }

    for (int i=0;i<num_cmds;i++){
        pids[i] = fork();
        if (pids[i]<0){ perror("fork failed"); exit(1); }

        if (pids[i]==0){
            /* input from previous pipe */
            if(i>0) dup2(pipefds[(i-1)*2], STDIN_FILENO);
            /* output to next pipe */ 
            if(i<num_cmds-1) dup2(pipefds[i*2+1], STDOUT_FILENO);

            /* close all pipes in child */
            for(int j=0;j<2*(num_cmds-1);j++) close(pipefds[j]);

            char *args[MAX_ARGS]; int argsc;
            parse_command(commands[i], args, &argsc);

            /* handle redirection for this command */ 
            for (int j=0; args[j]!=NULL; j++){
                if(strcmp(args[j], ">")==0 || strcmp(args[j], ">>")==0){
                    int append = (strcmp(args[j], ">>")==0);
                    char *filename = args[j+1];
                    int fd;
                    if(append) fd = open(filename, O_WRONLY|O_CREAT|O_APPEND,0644);
                    else fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC,0644);
                    if(fd<0){perror("open failed"); exit(1);}
                    dup2(fd, STDOUT_FILENO); close(fd);
                    args[j]=NULL;
                    break;
                }
            }
            execvp(args[ARG_PROGNAME], args);
            perror("execvp failed");
            exit(1);
        }
    }

    /* parent closes all pipes */ 
    for(int i=0;i<2*(num_cmds-1);i++) close(pipefds[i]);

    /* wait for all children */
    for(int i=0;i<num_cmds;i++) waitpid(pids[i], NULL, 0);
}

//========================================___BATCH___========================================

/* check for batched commands separated by ; only in the top level */
int command_with_batch(char line[]){
    int level = 0;
    for(int i=0; line[i]!='\0'; i++){
        if(line[i]=='(') level++;
        else if(line[i]==')') level--;
        else if(line[i]==';' && level==0) return 1;
    }
    return 0;
}

/* splits batched commands in the topmost layer*/
int split_batch(char line[], char *commands[]){
    int count=0, level=0;
    char *start=line;

    while(*start==' '||*start=='\t') start++; /* skip leading space */

    for(char *p=line;*p!='\0';p++){
        if(*p=='(') level++;
        else if(*p==')') level--;
        else if(*p==';' && level==0){
            *p='\0';
            char *end = p-1;
            while(end>start && isspace(*end)) *end--='\0';
            commands[count++] = start;
            start=p+1;
            while(*start==' '||*start=='\t') start++;
        }
    } /* add last command */
    if(*start!='\0' && count<MAX_ARGS-1){
        char *end=start+strlen(start)-1;
        while(end>start && isspace(*end)) *end--='\0';
        commands[count++] = start;
    }
    commands[count]=NULL;
    return count;
}

//========================================___SUBSHELL___========================================

/* nested subshells */
int command_with_subshell(char *line){
    for(int i=0; line[i]!='\0'; i++)
        if(line[i]=='(') return 1;
    return 0;
}

/* extract next subshell using stack approach for nested parentheses */
void extract_next_subshell(char *line, int start, int *sub_start, int *sub_end){
    int level=0;
    for(int i=start; line[i]!='\0'; i++){
        if(line[i]=='('){
            if(level==0){
                *sub_start=i;
                level++;
            }
        } else if(line[i]==')'){
            level--;
            if(level==0){
                *sub_end=i;
                return; 
            }
        }
    }
    *sub_start = *sub_end = -1;
}

/* launch subshell, supports nested commands */
void launch_subshell(char *line){
    int sub_start, sub_end;
    extract_next_subshell(line, 0, &sub_start, &sub_end);
    if(sub_start==-1){ fprintf(stderr,"No subshell found\n"); return; }

    char subcmd[MAX_LINE];
    strncpy(subcmd, line+sub_start+1, sub_end-sub_start-1);
    subcmd[sub_end-sub_start-1]='\0';

    pid_t pid=fork();
    if(pid<0){ perror("fork failed"); return; }
    if(pid==0){
        char sub_lwd[MAX_PROMPT_LEN-6];
        init_lwd(sub_lwd);

        if(command_with_batch(subcmd)){
            char *batch[MAX_ARGS];
            int batch_count=split_batch(subcmd,batch);
            for(int i=0;i<batch_count;i++) execute_command(batch[i], sub_lwd);
        } else execute_command(subcmd, sub_lwd);

        exit(0);
    }
    waitpid(pid,NULL,0);
}

//========================================___EXECUTE COMMAND___========================================

/* executes command */
void execute_command(char *cmd, char lwd[]){
    char copy[MAX_LINE];
    char *args[MAX_ARGS];
    int argsc;

    strcpy(copy,cmd);

    if(command_with_subshell(copy)){
        launch_subshell(copy);
    } else if(is_cd(copy)){
        parse_command(copy,args,&argsc);
        run_cd(args,argsc,lwd);
    } else if (command_with_pipe(copy)){
        char *pipe_cmds[MAX_ARGS];
        int num_cmds = split_pipeline(copy, pipe_cmds);
        launch_pipeline(pipe_cmds,num_cmds);
    } else if(command_with_redirection(copy)){
        parse_command(copy,args,&argsc);
        launch_program_with_redirection(args,argsc);
    } else{
        parse_command(copy,args,&argsc);
        launch_program(args,argsc);
    }
}
