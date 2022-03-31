#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#define buf_size 64
char* readstr(){
  int index=0;
  char* buf=(char*)malloc(buf_size);
  memset(buf,0,buf_size);
  while(read(0,buf+index,1)&&buf[index]!=' '&&buf[index]!='\n'){
    index++;
  }
  buf[index]=0;
  return buf;
}
int
main(int argc,char* argv[])
{
  int num=MAXARG;
  char* argv_[num];
  int i=0;
  for(;i<argc-1;i++){
    argv_[i]=(char*)malloc(buf_size);
    memset(argv_[i],0,buf_size);
    strcpy(argv_[i],argv[i+1]);
  }  
  while((argv_[i]=readstr())[0])
    i++;
  argv_[i]=(void*)(0);  
  if(argc>1){
    if(fork()==0){
      exec(argv[1],argv_);
      exit(0);
    }
    else{
      wait(0);
      exit(0);
    }
  }
  else{
    fprintf(1,"usage:program argv... | xargs program argv...\n");
    exit(1);
  }
}