#include "s3.h"

int main(int argc, char *argv[]) {
    char line[MAX_LINE];
    char lwd[MAX_PROMPT_LEN - 6];
    init_lwd(lwd);

    char *args[MAX_ARGS];
    int argsc;

    while (1) {
        read_command_line(line, lwd);

        // repeat for each batched command
        if (command_with_batch(line)) {
            char *batch[MAX_ARGS];
            int batch_count = split_batch(line, batch);

            for (int i = 0; i < batch_count; i++) {
                char *cmd = batch[i];

                if (is_cd(cmd)) {
                    parse_command(cmd, args, &argsc);
                    run_cd(args, argsc, lwd);
                } else if (command_with_pipe(cmd)) {
                    char *pipe_commands[MAX_ARGS];
                    int num_cmds = split_pipeline(cmd, pipe_commands);
                    launch_pipeline(pipe_commands, num_cmds);
                    reap();
                } else if (command_with_redirection(cmd)) {
                    parse_command(cmd, args, &argsc);
                    launch_program_with_redirection(args, argsc);
                    reap();
                } else {
                    parse_command(cmd, args, &argsc);
                    launch_program(args, argsc);
                    reap();
                }
            }

        } else {

            // non batched handling
            if (is_cd(line)) {
                parse_command(line, args, &argsc);
                run_cd(args, argsc, lwd);
            } else if (command_with_pipe(line)) {
                char *commands[MAX_ARGS];
                int num_cmds = split_pipeline(line, commands);
                launch_pipeline(commands, num_cmds);
                reap();
            } else if (command_with_redirection(line)) {
                parse_command(line, args, &argsc);
                launch_program_with_redirection(args, argsc);
                reap();
            } else {
                parse_command(line, args, &argsc);
                launch_program(args, argsc);
                reap();
            }
        }
    }

    return 0;
}
