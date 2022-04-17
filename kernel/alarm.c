#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
uint64 
sys_sigalarm(void)
{
  struct proc* p=myproc();
  int interval;
  uint64 handler;
  if(argint(0,&interval)<0)
    panic("sigalarm can't acquire time interval");
  if(argaddr(1,&handler)<0)  
    panic("sigalarm can't acquire handler");
  p->interval=interval;
  p->ticks_left=interval;
  p->handler=(handler_type)handler;
  return 0;
}
uint64 
sys_sigreturn(void)
{
  struct proc* p=myproc();
  *p->trapframe=*p->alarm_trapframe;
  p->re_entrant=0;
  return 0;
}