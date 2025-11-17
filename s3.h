#ifndef _S3_H_
#define _S3_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 128
#define MAX_PROMPT_LEN 256

/* improves readability when accessing args */
enum ArgIndex {
    ARG_PROGNAME,
    ARG_1,
    ARG_2,
    ARG_3
};

/* wait for single child to finish */
static inline void reap() {
    wait(NULL);
}

/* prompt and input */
void construct_shell_prompt(char shell_prompt[], char lwd[]);
void read_command_line(char line[], char lwd[]);
void parse_command(char line[], char *args[], int *argsc);

/* cd support built-in */
void init_lwd(char lwd[]);
int is_cd(char line[]);
void run_cd(char *args[], int argsc, char lwd[]);

/* basic execution */
void child(char *args[], int argsc);
void launch_program(char *args[], int argsc);

/* redirection */
void launch_program_with_redirection(char *args[], int argsc);
void child_with_output_redirected(char *args[], int argsc, char *filename, int output_mode);
void child_with_input_redirected(char *args[], int argsc, char *filename);
int command_with_redirection(char line[]);

/* pipe support */
int command_with_pipe(char line[]);
int split_pipeline(char line[], char *commands[]);
void launch_pipeline(char *commands[], int num_cmds);

/* batched commands */
int command_with_batch(char line[]);
int split_batch(char line[], char *commands[]);

/* subshells support (nested) */
int command_with_subshell(char *line);
void extract_next_subshell(char *line, int start, int *sub_start, int *sub_end);
void launch_subshell(char *line);

/* execute command */
void execute_command(char *cmd, char lwd[]);

#endif
