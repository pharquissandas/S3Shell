#include "s3.h"

int main(int argc, char *argv[]) {
    char line[MAX_LINE];
    char lwd[MAX_PROMPT_LEN - 6];
    init_lwd(lwd);

    char *args[MAX_ARGS];
    int argsc;

    while (1) {
        read_command_line(line, lwd);

        // make a copy to avoid modifying original
        char line_copy[MAX_LINE];
        strcpy(line_copy, line);

        // repeat for each batched command
        if (command_with_batch(line_copy)) {
            char *batch[MAX_ARGS];
            int batch_count = split_batch(line_copy, batch);

            for (int i = 0; i < batch_count; i++) {
                char *cmd = batch[i];
                execute_command(cmd, lwd);
            }
        } else {
            // non batched handling
            execute_command(line_copy, lwd);
        }
    }
    return 0;
}
