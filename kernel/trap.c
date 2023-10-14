#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}


// save rigisters
void tosaverigts(struct proc* nowproc){
  nowproc->saverigts.a0 = nowproc->trapframe->a0;
  nowproc->saverigts.a1 = nowproc->trapframe->a1;
  nowproc->saverigts.a2 = nowproc->trapframe->a2;
  nowproc->saverigts.a3 = nowproc->trapframe->a3;
  nowproc->saverigts.a4 = nowproc->trapframe->a4;
  nowproc->saverigts.a5 = nowproc->trapframe->a5;
  nowproc->saverigts.a6 = nowproc->trapframe->a6;
  nowproc->saverigts.a7 = nowproc->trapframe->a7;
  nowproc->saverigts.s0 = nowproc->trapframe->s0;
  nowproc->saverigts.s1 = nowproc->trapframe->s1;
  nowproc->saverigts.s2 = nowproc->trapframe->s2;
  nowproc->saverigts.s3 = nowproc->trapframe->s3;
  nowproc->saverigts.s4 = nowproc->trapframe->s4;
  nowproc->saverigts.s5 = nowproc->trapframe->s5;
  nowproc->saverigts.s6 = nowproc->trapframe->s6;
  nowproc->saverigts.s7 = nowproc->trapframe->s7;
  nowproc->saverigts.s8 = nowproc->trapframe->s8;
  nowproc->saverigts.s9 = nowproc->trapframe->s9;
  nowproc->saverigts.s10 = nowproc->trapframe->s10;
  nowproc->saverigts.s11 = nowproc->trapframe->s11;
  nowproc->saverigts.t0 = nowproc->trapframe->t0;
  nowproc->saverigts.t1 = nowproc->trapframe->t1;
  nowproc->saverigts.t2 = nowproc->trapframe->t2;
  nowproc->saverigts.t3 = nowproc->trapframe->t3;
  nowproc->saverigts.t4 = nowproc->trapframe->t4;
  nowproc->saverigts.t5 = nowproc->trapframe->t5;
  nowproc->saverigts.t6 = nowproc->trapframe->t6;
  nowproc->saverigts.epc = nowproc->trapframe->epc;
  nowproc->saverigts.ra = nowproc->trapframe->ra;
  nowproc->saverigts.sp = nowproc->trapframe->sp;
  nowproc->saverigts.gp = nowproc->trapframe->gp;
  nowproc->saverigts.tp = nowproc->trapframe->tp;  
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
    p->tickspased++;
    if(p->tickspased == p->alitvlnum) {
      p->tickspased = 0;
      p->alitvlnum = -1;
      tosaverigts(p);
      p->trapframe->epc = p->alfnpointer;
    }
    yield();
  }
  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

