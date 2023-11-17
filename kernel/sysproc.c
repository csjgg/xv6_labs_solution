#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"

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
sys_mmap(void){
  // get args
  uint64 addr;
  uint64 length;
  int prot;
  int flags;
  int fd;
  uint64 offset;
  argaddr(0, &addr);
  argaddr(1, &length);
  argint(2, &prot);
  argint(3, &flags);
  argint(4, &fd);
  argaddr(5, &offset);
  // alloc mem
  struct proc* p = myproc();
  acquire(&p->lock);
  addr = PGROUNDUP(p->sz);
  p->sz = PGROUNDUP(p->sz)+length;
  // get file
  struct file* f = p->ofile[fd];
  // check file open
  int ref = 0;
  for(int j = 0; j < 16; j++){
    if(p->vmas[j].used==1&&p->vmas[j].fd == f){
      ref++;
    }
  }
  if(ref == p->ofile[fd]->ref){
    release(&p->lock);
    return -1;
  }
  // check writable
  if(prot&PROT_WRITE&&p->ofile[fd]->writable==0){
    if(flags&MAP_PRIVATE){
    }else{
      release(&p->lock);
      return -1;
    }
  }
  filedup(f);
  // set vma
  // printf("%d,%x\n",fd,f);
  for(int i = 0; i < 16; i++){
    if(p->vmas[i].used!=1){
      p->vmas[i].used = 1;
      p->vmas[i].addr = addr;
      p->vmas[i].fd = f;
      p->vmas[i].flags = flags;
      p->vmas[i].length = length;
      p->vmas[i].offset = offset;
      p->vmas[i].prot = prot;
      release(&p->lock);
      return addr;
    }
  }
  release(&p->lock);
  return -1;  
}

uint64
sys_munmap(void){
  uint64 addr;
  uint64 length;
  argaddr(0,&addr);
  argaddr(1,&length);
  struct proc* p = myproc();
  int i,flllag = 0;
  // find vma
  for(i = 0; i < 16; i++){
    if(p->vmas[i].used == 1 && p->vmas[i].addr <= addr && p->vmas[i].length+p->vmas[i].addr>=addr){
      flllag = 1;
      break;
    }
  }
  int closefile = 0;
  if(flllag == 0){
    return -1;
  }else{
    // do not handle
    if(addr > p->vmas[i].addr && addr + length < p->vmas[i].addr + p->vmas[i].length){
      return -1;
    }
    uint64 oldaddr = addr;
    addr = PGROUNDDOWN(addr);
    uint64 min = oldaddr+length;
    if(min>p->vmas[i].addr+p->vmas[i].length){
      min = p->vmas[i].addr+p->vmas[i].length;
    }
    // deal with vma
    if(addr > p->vmas[i].addr){
      p->vmas[i].length = addr-p->vmas[i].addr;
    }else{
      uint64 tmp = PGROUNDUP(oldaddr+length);
      if(p->vmas[i].addr + p->vmas[i].length <= tmp){
        p->vmas[i].used = 0;
        p->vmas[i].length = 0;
        // deal with file
        // fileclose(p->vmas[i].fd);
        closefile = 1;
      }else{
        p->vmas[i].length =p->vmas[i].addr + p->vmas[i].length - tmp; 
        p->vmas[i].addr = tmp;
      }
    }
    // deal with mem
    for(addr = PGROUNDDOWN(addr); addr < min; addr+=4096){
      pte_t *ppp ;
      if((ppp = walk(p->pagetable,addr,0))!=0){
        if(*ppp&PTE_V){
          filewrite(p->vmas[i].fd,addr,4096);
          uvmunmap(p->pagetable,addr,1,1);
        }
      }
    }
  }
  if(closefile == 1){
    fileclose(p->vmas[i].fd);
  }
  return 0;
}