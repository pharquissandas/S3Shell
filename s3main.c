#include "s3.h"

int main(int argc, char *argv[]){
    char line[MAX_LINE];  /* stores command line input */
    char lwd[MAX_PROMPT_LEN - 6];  /* last working directory for cd - */
    init_lwd(lwd);

    while (1) {
        read_command_line(line, lwd);  /* gets input from user */

        /* skip empty lines */
        if (line[0] == '\0') continue;

        /* check for batch commands separated by semicolons */
        if(command_with_batch(line)){
            char *batch[MAX_ARGS];
            int batch_count=split_batch(line,batch);
            
            for(int i=0; i<batch_count; i++){
                execute_command(batch[i],lwd);
            }
        }
        else{
            execute_command(line,lwd);
        }
    }
    return 0;
}
