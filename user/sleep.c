#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    if(argc!=2){
        fprintf(2,"usage: sleep time[ms]\n");
        exit(1);
    }
    int time_to_sleep=atoi(argv[1]);
    if(time_to_sleep>0)
        sleep(time_to_sleep);
    exit(0);    
}