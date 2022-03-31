#include "kernel/types.h"
#include "user/user.h"
// int 
// main(int argc,char* argv[])  //Non-pipeline mode
// {
//     int pips[16][2];
//     int index=0;
//     int can_fork=0;
//     while(1){
//         can_fork=0;
//         pipe(pips[index]);
//         if(index==0){
//             for(int i=2;i<=35;i++)
//                 write(pips[index][1],&i,4);    
//             can_fork=1;     
//         }
//         else{
//             int prime=0,from_left;
//             read(pips[index-1][0],&prime,4);
//             printf("%d\n",prime);
//             while(read(pips[index-1][0],&from_left,4)){
//                 if(from_left%prime!=0){
//                     write(pips[index][1],&from_left,4);
//                     can_fork=1;
//                 }
//             }
//             close(pips[index-1][0]);
//         }
//         close(pips[index][1]);
//         if(can_fork&&fork()==0){ 
//             index++;
//         }
//         else{
//             close(pips[index][0]);
//             wait(0);
//             exit(0);
//         }
//     }
//     return 0;
// }
int 
main(int argc,char* argv[])  //pipeline mode
{
    int pips[16][2];
    int index=0;
    int has_forked=0;
    while(1){
    Loop:   
        has_forked=0;
        pipe(pips[index]);
        if(index==0){
            for(int i=2;i<=35;i++){
                write(pips[index][1],&i,4); 
                if(!has_forked){
                    has_forked=1;
                    if(fork()==0){
                        index++;
                        goto Loop;
                    }
                }
            }      
        }
        else{
            close(pips[index-1][1]);
            int prime=0,from_left;
            read(pips[index-1][0],&prime,4);
            printf("prime %d\n",prime);
            while(read(pips[index-1][0],&from_left,4)){
                if(from_left%prime!=0){
                    write(pips[index][1],&from_left,4);
                    if(!has_forked){
                        has_forked=1;
                        if(fork()==0){
                            index++;
                            goto Loop;
                        }
                    }
                }
            }
            close(pips[index-1][0]);
        }
        close(pips[index][1]);
        close(pips[index][0]);
        wait(0);
        exit(0);
    }
    return 0;
}
