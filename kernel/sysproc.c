#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;
  backtrace();
  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_sigalarm(void){
  int interval;
  uint64 fnpointer;
  argint(0, &interval);
  argaddr(1, &fnpointer);
  struct proc* mypr = myproc();
  mypr->alfnpointer = fnpointer;
  mypr->alitvlnum = interval;
  mypr->alitvlnumsave = interval;
  return 0;
}


void resumerigts(struct proc* nowproc){
  nowproc->trapframe->epc = nowproc->saverigts.epc;
  nowproc->trapframe->a0 = nowproc->saverigts.a0;
  nowproc->trapframe->a1 = nowproc->saverigts.a1;
  nowproc->trapframe->a2 = nowproc->saverigts.a2;
  nowproc->trapframe->a3 = nowproc->saverigts.a3;
  nowproc->trapframe->a4 = nowproc->saverigts.a4;
  nowproc->trapframe->a5 = nowproc->saverigts.a5;
  nowproc->trapframe->a6 = nowproc->saverigts.a6;
  nowproc->trapframe->a7 = nowproc->saverigts.a7;
  nowproc->trapframe->t0 = nowproc->saverigts.t0;
  nowproc->trapframe->t1 = nowproc->saverigts.t1;
  nowproc->trapframe->t2 = nowproc->saverigts.t2;
  nowproc->trapframe->t3 = nowproc->saverigts.t3;
  nowproc->trapframe->t4 = nowproc->saverigts.t4;
  nowproc->trapframe->t5 = nowproc->saverigts.t5;
  nowproc->trapframe->t6 = nowproc->saverigts.t6;
  nowproc->trapframe->s0 = nowproc->saverigts.s0;
  nowproc->trapframe->s1 = nowproc->saverigts.s1;
  nowproc->trapframe->s2 = nowproc->saverigts.s2;
  nowproc->trapframe->s3 = nowproc->saverigts.s3;
  nowproc->trapframe->s4 = nowproc->saverigts.s4;
  nowproc->trapframe->s5 = nowproc->saverigts.s5;
  nowproc->trapframe->s6 = nowproc->saverigts.s6;
  nowproc->trapframe->s7 = nowproc->saverigts.s7;
  nowproc->trapframe->s8 = nowproc->saverigts.s8;
  nowproc->trapframe->s9 = nowproc->saverigts.s9;
  nowproc->trapframe->s10 = nowproc->saverigts.s10;
  nowproc->trapframe->s11 = nowproc->saverigts.s11;
  nowproc->trapframe->ra = nowproc->saverigts.ra;
  nowproc->trapframe->sp = nowproc->saverigts.sp;
  nowproc->trapframe->gp = nowproc->saverigts.gp;
  nowproc->trapframe->tp = nowproc->saverigts.tp;
}


uint64
sys_sigreturn(void){
  struct proc* nowproc = myproc();
  resumerigts(nowproc);
  nowproc->alitvlnum = nowproc->alitvlnumsave;
  return nowproc->saverigts.a0;
}
