// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);


struct spinlock pgreflock;
int pgref[PHYSTOP/PGSIZE]; // number of pointers to each physical page

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&pgreflock, "pgref");
  memset(pgref, 0, PHYSTOP/PGSIZE*sizeof(int));
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    pgref[(uint64)p/PGSIZE] = 0;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&pgreflock);
  if(--pgref[(uint64)pa/PGSIZE]>0){
    release(&pgreflock);
    return;
  }
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  pgref[(uint64)pa/PGSIZE] = 0;
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
  release(&pgreflock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);
  if (r){
    pgref[(uint64)r/PGSIZE] = 1;
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


void addref(uint64 pa){
  acquire(&pgreflock);
  pgref[pa/PGSIZE]++;
  release(&pgreflock);
}


uint64 retnewpage(uint64 pa){
  acquire(&pgreflock);
  if(pgref[pa/PGSIZE] <= 1){
    release(&pgreflock);
    return pa;
  }
  uint64 pa1 = (uint64)kalloc();
  if(pa1 == 0){
    release(&pgreflock);
    return 0;
  }
  memmove((void*)pa1, (void*)pa, PGSIZE);
  pgref[pa/PGSIZE]--;
  release(&pgreflock);
  return pa1;
}


