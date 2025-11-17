#include "s3.h"

int main(int argc, char *argv[]){
    char line[MAX_LINE];
    char lwd[MAX_PROMPT_LEN - 6];
    init_lwd(lwd);

    char *args[MAX_ARGS];
    int argsc;

    while (1) {
        read_command_line(line, lwd);

        if(command_with_batch(line)){
            char *batch[MAX_ARGS];
            int batch_count=split_batch(line,batch);
            for(int i=0; i<batch_count; i++) execute_command(batch[i],lwd);
        } 
        else{
            execute_command(line,lwd);
        }
    }
    return 0;
}
