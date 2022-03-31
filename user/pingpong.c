#include "kernel/types.h"
#include "user/user.h"

int
main(int argc,char* argv[])
{
    int p[2];
    char buf[10];
    buf[0]='a';
    pipe(p);
    if(fork()==0){//child received ping and write back the byte
        if(read(p[0],buf,1)>0){
            fprintf(1,"%d: received ping\n",getpid());
            write(p[1],buf,1);
            exit(0);
        }
    }
    else{
        write(p[1],buf,1);
        wait(0);
        if(read(p[0],buf,1)>0){
            fprintf(1,"%d: received pong\n",getpid());
            exit(0);
        }
    }
    return 0;
}